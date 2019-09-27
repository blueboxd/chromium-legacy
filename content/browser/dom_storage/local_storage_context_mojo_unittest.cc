// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/local_storage_context_mojo.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "build/build_config.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "content/browser/dom_storage/dom_storage_database.h"
#include "content/browser/dom_storage/dom_storage_task_runner.h"
#include "content/browser/dom_storage/dom_storage_types.h"
#include "content/browser/dom_storage/test/fake_leveldb_database_error_on_write.h"
#include "content/browser/dom_storage/test/fake_leveldb_service.h"
#include "content/browser/dom_storage/test/storage_area_test_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "content/test/fake_leveldb_database.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

using leveldb::StdStringToUint8Vector;
using leveldb::Uint8VectorToStdString;

namespace content {

namespace {
using test::FakeLevelDBService;
using test::FakeLevelDBDatabaseErrorOnWrite;

void GetStorageUsageCallback(const base::RepeatingClosure& callback,
                             std::vector<StorageUsageInfo>* out_result,
                             std::vector<StorageUsageInfo> result) {
  *out_result = std::move(result);
  callback.Run();
}

class TestLevelDBObserver : public blink::mojom::StorageAreaObserver {
 public:
  struct Observation {
    enum { kAdd, kChange, kDelete, kDeleteAll } type;
    std::string key;
    std::string old_value;
    std::string new_value;
    std::string source;
  };

  TestLevelDBObserver() = default;

  mojo::PendingAssociatedRemote<blink::mojom::StorageAreaObserver> Bind() {
    return receiver_.BindNewEndpointAndPassRemote();
  }

  const std::vector<Observation>& observations() { return observations_; }

 private:
  void KeyAdded(const std::vector<uint8_t>& key,
                const std::vector<uint8_t>& value,
                const std::string& source) override {
    observations_.push_back({Observation::kAdd, Uint8VectorToStdString(key), "",
                             Uint8VectorToStdString(value), source});
  }
  void KeyChanged(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& new_value,
                  const std::vector<uint8_t>& old_value,
                  const std::string& source) override {
    observations_.push_back({Observation::kChange, Uint8VectorToStdString(key),
                             Uint8VectorToStdString(old_value),
                             Uint8VectorToStdString(new_value), source});
  }
  void KeyDeleted(const std::vector<uint8_t>& key,
                  const std::vector<uint8_t>& old_value,
                  const std::string& source) override {
    observations_.push_back({Observation::kDelete, Uint8VectorToStdString(key),
                             Uint8VectorToStdString(old_value), "", source});
  }
  void AllDeleted(const std::string& source) override {
    observations_.push_back({Observation::kDeleteAll, "", "", "", source});
  }
  void ShouldSendOldValueOnMutations(bool value) override {}

  std::vector<Observation> observations_;
  mojo::AssociatedReceiver<blink::mojom::StorageAreaObserver> receiver_{this};
};

}  // namespace

class LocalStorageContextMojoTest : public testing::Test {
 public:
  LocalStorageContextMojoTest()
      : db_(&mock_data_),
        db_receiver_(&db_),
        task_runner_(new MockDOMStorageTaskRunner(
            base::ThreadTaskRunnerHandle::Get().get())),
        mock_special_storage_policy_(new MockSpecialStoragePolicy()) {
    EXPECT_TRUE(temp_path_.CreateUniqueTempDir());
  }

  ~LocalStorageContextMojoTest() override {
    if (context_)
      ShutdownContext();

    EXPECT_TRUE(temp_path_.Delete());
  }

  LocalStorageContextMojo* context() {
    if (!context_) {
      context_ = new LocalStorageContextMojo(
          temp_path_.GetPath(), base::ThreadTaskRunnerHandle::Get(),
          task_runner_, temp_path_.GetPath(),
          base::FilePath(FILE_PATH_LITERAL("leveldb")),
          special_storage_policy());
      mojo::AssociatedRemote<leveldb::mojom::LevelDBDatabase> database_remote;
      auto receiver =
          database_remote.BindNewEndpointAndPassDedicatedReceiverForTesting();
      context_->SetDatabaseForTesting(database_remote.Unbind());
      db_receiver_.Bind(std::move(receiver));
    }
    return context_;
  }

  void ShutdownContext() {
    context_->ShutdownAndDelete();
    context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  MockSpecialStoragePolicy* special_storage_policy() {
    return mock_special_storage_policy_.get();
  }

  const std::map<std::vector<uint8_t>, std::vector<uint8_t>>& mock_data() {
    return mock_data_;
  }

  void clear_mock_data() { mock_data_.clear(); }

  void set_mock_data(const std::string& key, const std::string& value) {
    mock_data_[StdStringToUint8Vector(key)] = StdStringToUint8Vector(value);
  }

  std::vector<StorageUsageInfo> GetStorageUsageSync() {
    base::RunLoop run_loop;
    std::vector<StorageUsageInfo> result;
    context()->GetStorageUsage(base::BindOnce(&GetStorageUsageCallback,
                                              run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  base::Optional<std::vector<uint8_t>> DoTestGet(
      const std::vector<uint8_t>& key) {
    const url::Origin kOrigin = url::Origin::Create(GURL("http://foobar.com"));
    mojo::Remote<blink::mojom::StorageArea> area;
    mojo::Remote<blink::mojom::StorageArea>
        dummy_area;  // To make sure values are cached.
    context()->OpenLocalStorage(kOrigin, area.BindNewPipeAndPassReceiver());
    context()->OpenLocalStorage(kOrigin,
                                dummy_area.BindNewPipeAndPassReceiver());
    std::vector<uint8_t> result;
    bool success = test::GetSync(area.get(), key, &result);
    return success ? base::Optional<std::vector<uint8_t>>(result)
                   : base::nullopt;
  }

  void CloseBinding() { db_receiver_.reset(); }

  const base::FilePath& temp_path() const { return temp_path_.GetPath(); }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void DoTestPut(LocalStorageContextMojo* context,
                 const std::vector<uint8_t>& key,
                 const std::vector<uint8_t>& value) {
    mojo::Remote<blink::mojom::StorageArea> area;
    bool success = false;
    base::RunLoop run_loop;
    context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());
    area->Put(key, value, base::nullopt, "source",
              test::MakeSuccessCallback(run_loop.QuitClosure(), &success));
    run_loop.Run();
    EXPECT_TRUE(success);
    area.reset();
    RunUntilIdle();
  }

  bool DoTestGet(LocalStorageContextMojo* context,
                 const std::vector<uint8_t>& key,
                 std::vector<uint8_t>* result) {
    mojo::Remote<blink::mojom::StorageArea> area;
    context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());

    base::RunLoop run_loop;
    std::vector<blink::mojom::KeyValuePtr> data;
    bool success = false;
    bool done = false;
    area->GetAll(
        test::GetAllCallback::CreateAndBind(&done, run_loop.QuitClosure()),
        test::MakeGetAllCallback(&success, &data));
    run_loop.Run();
    EXPECT_TRUE(done);
    EXPECT_TRUE(success);

    for (auto& entry : data) {
      if (key == entry->key) {
        *result = std::move(entry->value);
        return true;
      }
    }
    result->clear();
    return false;
  }

  base::FilePath FirstEntryInDir() {
    base::FileEnumerator enumerator(
        temp_path(), false /* recursive */,
        base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
    return enumerator.Next();
  }

 private:
  // testing::Test:
  void TearDown() override {
    // Some of these tests close message pipes which serve as master interfaces
    // to other associated interfaces; this in turn schedules tasks to invoke
    // the associated interfaces' error handlers, and local storage code relies
    // on those handlers running in order to avoid memory leaks at shutdown.
    RunUntilIdle();
  }

  BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_path_;
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> mock_data_;
  FakeLevelDBDatabase db_;
  mojo::AssociatedReceiver<leveldb::mojom::LevelDBDatabase> db_receiver_;

  scoped_refptr<MockDOMStorageTaskRunner> task_runner_;

  LocalStorageContextMojo* context_ = nullptr;

  scoped_refptr<MockSpecialStoragePolicy> mock_special_storage_policy_;

  DISALLOW_COPY_AND_ASSIGN(LocalStorageContextMojoTest);
};

TEST_F(LocalStorageContextMojoTest, Basic) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  base::RunLoop().RunUntilIdle();

  // Should have three rows of data, one for the version, one for the actual
  // data and one for metadata.
  ASSERT_EQ(3u, mock_data().size());
}

TEST_F(LocalStorageContextMojoTest, OriginsAreIndependent) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com:123"));
  url::Origin origin2 = url::Origin::Create(GURL("http://foobar.com:1234"));
  auto key1 = StdStringToUint8Vector("4key");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->Put(key1, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key2, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(5u, mock_data().size());
}

TEST_F(LocalStorageContextMojoTest, WrapperOutlivesMojoConnection) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  // Write some data to the DB.
  mojo::Remote<blink::mojom::StorageArea> area;
  mojo::Remote<blink::mojom::StorageArea>
      dummy_area;  // To make sure values are cached.
  const url::Origin kOrigin(url::Origin::Create(GURL("http://foobar.com")));
  context()->OpenLocalStorage(kOrigin, area.BindNewPipeAndPassReceiver());
  context()->OpenLocalStorage(kOrigin, dummy_area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();
  dummy_area.reset();
  base::RunLoop().RunUntilIdle();

  // Clear all the data from the backing database.
  EXPECT_FALSE(mock_data().empty());
  clear_mock_data();

  // Data should still be readable, because despite closing the area
  // connection above, the actual area instance should have been kept alive.
  EXPECT_EQ(value, DoTestGet(key));

  // Now purge memory.
  context()->PurgeMemory();

  // And make sure caches were actually cleared.
  EXPECT_EQ(base::nullopt, DoTestGet(key));
}

TEST_F(LocalStorageContextMojoTest, OpeningWrappersPurgesInactiveWrappers) {
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  // Write some data to the DB.
  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();
  base::RunLoop().RunUntilIdle();

  // Clear all the data from the backing database.
  EXPECT_FALSE(mock_data().empty());
  clear_mock_data();

  // Now open many new areas (for different origins) to trigger clean up.
  for (int i = 1; i <= 100; ++i) {
    context()->OpenLocalStorage(url::Origin::Create(GURL(base::StringPrintf(
                                    "http://example.com:%d", i))),
                                area.BindNewPipeAndPassReceiver());
    area.reset();
  }

  // And make sure caches were actually cleared.
  EXPECT_EQ(base::nullopt, DoTestGet(key));
}

TEST_F(LocalStorageContextMojoTest, ValidVersion) {
  set_mock_data("VERSION", "1");
  set_mock_data(std::string("_http://foobar.com") + '\x00' + "key", "value");

  EXPECT_EQ(StdStringToUint8Vector("value"),
            DoTestGet(StdStringToUint8Vector("key")));
}

TEST_F(LocalStorageContextMojoTest, InvalidVersion) {
  set_mock_data("VERSION", "foobar");
  set_mock_data(std::string("_http://foobar.com") + '\x00' + "key", "value");

  EXPECT_EQ(base::nullopt, DoTestGet(StdStringToUint8Vector("key")));
}

TEST_F(LocalStorageContextMojoTest, VersionOnlyWrittenOnCommit) {
  EXPECT_EQ(base::nullopt, DoTestGet(StdStringToUint8Vector("key")));

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_data().empty());
}

TEST_F(LocalStorageContextMojoTest, GetStorageUsage_NoData) {
  std::vector<StorageUsageInfo> info = GetStorageUsageSync();
  EXPECT_EQ(0u, info.size());
}

TEST_F(LocalStorageContextMojoTest, GetStorageUsage_Data) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key1 = StdStringToUint8Vector("key1");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  base::Time before_write = base::Time::Now();

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->Put(key1, value, base::nullopt, "source", base::DoNothing());
  area->Put(key2, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key2, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  // GetStorageUsage only includes committed data, but still returns all origins
  // that used localstorage with zero size.
  std::vector<StorageUsageInfo> info = GetStorageUsageSync();
  ASSERT_EQ(2u, info.size());
  if (info[0].origin == origin2)
    std::swap(info[0], info[1]);
  EXPECT_EQ(origin1, info[0].origin);
  EXPECT_EQ(origin2, info[1].origin);
  EXPECT_LE(before_write, info[0].last_modified);
  EXPECT_LE(before_write, info[1].last_modified);
  EXPECT_EQ(0u, info[0].total_size_bytes);
  EXPECT_EQ(0u, info[1].total_size_bytes);

  // Make sure all data gets committed to disk.
  base::RunLoop().RunUntilIdle();

  base::Time after_write = base::Time::Now();

  info = GetStorageUsageSync();
  ASSERT_EQ(2u, info.size());
  if (info[0].origin == origin2)
    std::swap(info[0], info[1]);
  EXPECT_EQ(origin1, info[0].origin);
  EXPECT_EQ(origin2, info[1].origin);
  EXPECT_LE(before_write, info[0].last_modified);
  EXPECT_LE(before_write, info[1].last_modified);
  EXPECT_GE(after_write, info[0].last_modified);
  EXPECT_GE(after_write, info[1].last_modified);
  EXPECT_GT(info[0].total_size_bytes, info[1].total_size_bytes);
}

TEST_F(LocalStorageContextMojoTest, MetaDataClearedOnDelete) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();
  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->Delete(key, value, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  base::RunLoop().RunUntilIdle();

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  EXPECT_EQ(3u, mock_data().size());
  for (const auto& it : mock_data()) {
    if (Uint8VectorToStdString(it.first) == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin1.Serialize()));
    EXPECT_NE(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, MetaDataClearedOnDeleteAll) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();
  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->DeleteAll("source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  base::RunLoop().RunUntilIdle();

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  EXPECT_EQ(3u, mock_data().size());
  for (const auto& it : mock_data()) {
    if (Uint8VectorToStdString(it.first) == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin1.Serialize()));
    EXPECT_NE(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, MojoConnectionDisconnects) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  {
    mojo::Remote<blink::mojom::StorageArea> area;
    context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
    area->Put(key, value, base::nullopt, "source", base::DoNothing());
    area.reset();
  }
  EXPECT_EQ(value, DoTestGet(key));

  // Close the database connection.
  CloseBinding();
  base::RunLoop().RunUntilIdle();

  // We can't access the data anymore.
  EXPECT_EQ(base::nullopt, DoTestGet(key));

  // Check that local storage still works without a database.
  {
    mojo::Remote<blink::mojom::StorageArea> area;
    context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
    area->Put(key, value, base::nullopt, "source", base::DoNothing());
    area.reset();
  }
  EXPECT_EQ(value, DoTestGet(key));
}

TEST_F(LocalStorageContextMojoTest, DeleteStorage) {
  set_mock_data("VERSION", "1");
  set_mock_data(std::string("_http://foobar.com") + '\x00' + "key", "value");

  base::RunLoop run_loop;
  context()->DeleteStorage(url::Origin::Create(GURL("http://foobar.com")),
                           run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(1u, mock_data().size());
}

TEST_F(LocalStorageContextMojoTest, DeleteStorageWithoutConnection) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(mock_data().empty());

  context()->DeleteStorage(origin1, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  EXPECT_EQ(3u, mock_data().size());
  for (const auto& it : mock_data()) {
    if (Uint8VectorToStdString(it.first) == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin1.Serialize()));
    EXPECT_NE(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, DeleteStorageNotifiesWrapper) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(mock_data().empty());

  TestLevelDBObserver observer;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->AddObserver(observer.Bind());
  base::RunLoop().RunUntilIdle();

  context()->DeleteStorage(origin1, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(1u, observer.observations().size());
  EXPECT_EQ(TestLevelDBObserver::Observation::kDeleteAll,
            observer.observations()[0].type);

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  EXPECT_EQ(3u, mock_data().size());
  for (const auto& it : mock_data()) {
    if (Uint8VectorToStdString(it.first) == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin1.Serialize()));
    EXPECT_NE(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, DeleteStorageWithPendingWrites) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to disk.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(mock_data().empty());

  TestLevelDBObserver observer;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->AddObserver(observer.Bind());
  area->Put(StdStringToUint8Vector("key2"), value, base::nullopt, "source",
            base::DoNothing());
  base::RunLoop().RunUntilIdle();

  context()->DeleteStorage(origin1, base::DoNothing());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(2u, observer.observations().size());
  EXPECT_EQ(TestLevelDBObserver::Observation::kAdd,
            observer.observations()[0].type);
  EXPECT_EQ(TestLevelDBObserver::Observation::kDeleteAll,
            observer.observations()[1].type);

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  EXPECT_EQ(3u, mock_data().size());
  for (const auto& it : mock_data()) {
    if (Uint8VectorToStdString(it.first) == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin1.Serialize()));
    EXPECT_NE(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, Migration) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  base::string16 key = base::ASCIIToUTF16("key");
  base::string16 value = base::ASCIIToUTF16("value");
  base::string16 key2 = base::ASCIIToUTF16("key2");
  key2.push_back(0xd83d);
  key2.push_back(0xde00);

  base::FilePath old_db_path = temp_path().Append(
      LocalStorageContextMojo::LegacyDatabaseFileNameFromOrigin(origin1));
  {
    DOMStorageDatabase db(old_db_path);
    DOMStorageValuesMap data;
    data[key] = base::NullableString16(value, false);
    data[key2] = base::NullableString16(value, false);
    db.CommitChanges(false, data);
  }
  EXPECT_TRUE(base::PathExists(old_db_path));

  // Opening origin2 and accessing its data should not migrate anything.
  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());

  // To make sure values are cached.
  mojo::Remote<blink::mojom::StorageArea> dummy_area;
  context()->OpenLocalStorage(origin2, dummy_area.BindNewPipeAndPassReceiver());

  area->Get(std::vector<uint8_t>(), base::DoNothing());
  area.reset();
  dummy_area.reset();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(mock_data().empty());

  // Opening origin1 and accessing its data should migrate its storage.
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  context()->OpenLocalStorage(origin1, dummy_area.BindNewPipeAndPassReceiver());
  area->Get(std::vector<uint8_t>(), base::DoNothing());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(mock_data().empty());

  {
    std::vector<uint8_t> result;
    bool success = test::GetSync(
        area.get(), LocalStorageContextMojo::MigrateString(key), &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(LocalStorageContextMojo::MigrateString(value), result);
  }

  {
    std::vector<uint8_t> result;
    bool success = test::GetSync(
        area.get(), LocalStorageContextMojo::MigrateString(key2), &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(LocalStorageContextMojo::MigrateString(value), result);
  }

  // Origin1 should no longer exist in old storage.
  EXPECT_FALSE(base::PathExists(old_db_path));
}

static std::string EncodeKeyAsUTF16(const std::string& origin,
                                    const base::string16& key) {
  std::string result = '_' + origin + '\x00' + '\x00';
  std::copy(reinterpret_cast<const char*>(key.data()),
            reinterpret_cast<const char*>(key.data()) +
                key.size() * sizeof(base::char16),
            std::back_inserter(result));
  return result;
}

TEST_F(LocalStorageContextMojoTest, FixUp) {
  set_mock_data("VERSION", "1");
  // Add mock data for the "key" key, with both possible encodings for key.
  // We expect the value of the correctly encoded key to take precedence over
  // the incorrectly encoded key (and expect the incorrectly encoded key to be
  // deleted.
  set_mock_data(std::string("_http://foobar.com") + '\x00' + "\x01key",
                "value1");
  set_mock_data(
      EncodeKeyAsUTF16("http://foobar.com", base::ASCIIToUTF16("key")),
      "value2");
  // Also add mock data for the "foo" key, this time only with the incorrec
  // encoding. This should be updated to the correct encoding.
  set_mock_data(
      EncodeKeyAsUTF16("http://foobar.com", base::ASCIIToUTF16("foo")),
      "value3");

  mojo::Remote<blink::mojom::StorageArea> area;
  mojo::Remote<blink::mojom::StorageArea>
      dummy_area;  // To make sure values are cached.
  context()->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());
  context()->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              dummy_area.BindNewPipeAndPassReceiver());

  {
    std::vector<uint8_t> result;
    bool success = test::GetSync(
        area.get(), leveldb::StdStringToUint8Vector("\x01key"), &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(leveldb::StdStringToUint8Vector("value1"), result);
  }
  {
    std::vector<uint8_t> result;
    bool success = test::GetSync(area.get(),
                                 leveldb::StdStringToUint8Vector("\x01"
                                                                 "foo"),
                                 &result);
    EXPECT_TRUE(success);
    EXPECT_EQ(leveldb::StdStringToUint8Vector("value3"), result);
  }

  // Expect 4 rows in the database: VERSION, meta-data for the origin, and two
  // rows of actual data.
  EXPECT_EQ(4u, mock_data().size());
  EXPECT_EQ(leveldb::StdStringToUint8Vector("value1"),
            mock_data().rbegin()->second);
  EXPECT_EQ(leveldb::StdStringToUint8Vector("value3"),
            std::next(mock_data().rbegin())->second);
}

TEST_F(LocalStorageContextMojoTest, ShutdownClearsData) {
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  auto key1 = StdStringToUint8Vector("key1");
  auto key2 = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context()->OpenLocalStorage(origin1, area.BindNewPipeAndPassReceiver());
  area->Put(key1, value, base::nullopt, "source", base::DoNothing());
  area->Put(key2, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  context()->OpenLocalStorage(origin2, area.BindNewPipeAndPassReceiver());
  area->Put(key2, value, base::nullopt, "source", base::DoNothing());
  area.reset();

  // Make sure all data gets committed to the DB.
  base::RunLoop().RunUntilIdle();

  special_storage_policy()->AddSessionOnly(origin1.GetURL());
  ShutdownContext();

  // Data from origin2 should exist, including meta-data, but nothing should
  // exist for origin1.
  EXPECT_EQ(3u, mock_data().size());
  for (const auto& it : mock_data()) {
    if (Uint8VectorToStdString(it.first) == "VERSION")
      continue;
    EXPECT_EQ(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin1.Serialize()));
    EXPECT_NE(std::string::npos,
              Uint8VectorToStdString(it.first).find(origin2.Serialize()));
  }
}

TEST_F(LocalStorageContextMojoTest, InMemory) {
  auto* context = new LocalStorageContextMojo(
      base::FilePath(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), base::FilePath(), nullptr);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area.BindNewPipeAndPassReceiver());
  DoTestPut(context, key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Should not have created any files.
  EXPECT_TRUE(FirstEntryInDir().empty());

  // Re-opening should get fresh data.
  context = new LocalStorageContextMojo(
      base::FilePath(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), base::FilePath(), nullptr);
  EXPECT_FALSE(DoTestGet(context, key, &result));
  context->ShutdownAndDelete();
}

TEST_F(LocalStorageContextMojoTest, InMemoryInvalidPath) {
  auto* context = new LocalStorageContextMojo(
      base::FilePath(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), base::FilePath(FILE_PATH_LITERAL("../../")), nullptr);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  mojo::Remote<blink::mojom::StorageArea> area;
  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area.BindNewPipeAndPassReceiver());

  DoTestPut(context, key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Should not have created any files.
  EXPECT_TRUE(FirstEntryInDir().empty());
}

TEST_F(LocalStorageContextMojoTest, OnDisk) {
  base::FilePath test_path(FILE_PATH_LITERAL("test_path"));
  auto* context = new LocalStorageContextMojo(
      temp_path(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), test_path, nullptr);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(context, key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Should have created files.
  EXPECT_EQ(test_path, FirstEntryInDir().BaseName());

  // Should be able to re-open.
  context = new LocalStorageContextMojo(
      temp_path(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), test_path, nullptr);
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);
  context->ShutdownAndDelete();
}

TEST_F(LocalStorageContextMojoTest, InvalidVersionOnDisk) {
  base::FilePath test_path(FILE_PATH_LITERAL("test_path"));

  // Create context and add some data to it.
  auto* context = new LocalStorageContextMojo(
      temp_path(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), test_path, nullptr);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(context, key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  {
    // Mess up version number in database.
    leveldb_env::ChromiumEnv env;
    std::unique_ptr<leveldb::DB> db;
    leveldb_env::Options options;
    options.env = &env;
    base::FilePath db_path =
        temp_path().Append(test_path).Append(FILE_PATH_LITERAL("leveldb"));
    ASSERT_TRUE(leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db).ok());
    ASSERT_TRUE(db->Put(leveldb::WriteOptions(), "VERSION", "argh").ok());
  }

  // Make sure data is gone.
  context = new LocalStorageContextMojo(temp_path(),
                                        base::ThreadTaskRunnerHandle::Get(),
                                        nullptr, base::FilePath(),

                                        test_path, nullptr);
  EXPECT_FALSE(DoTestGet(context, key, &result));

  // Write data again.
  DoTestPut(context, key, value);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Data should have been preserved now.
  context = new LocalStorageContextMojo(
      temp_path(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), test_path, nullptr);
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);
  context->ShutdownAndDelete();
}

TEST_F(LocalStorageContextMojoTest, CorruptionOnDisk) {
  base::FilePath test_path(FILE_PATH_LITERAL("test_path"));

  // Create context and add some data to it.
  auto* context = new LocalStorageContextMojo(
      temp_path(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), test_path, nullptr);
  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  DoTestPut(context, key, value);
  std::vector<uint8_t> result;
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Delete manifest files to mess up opening DB.
  base::FilePath db_path =
      temp_path().Append(test_path).Append(FILE_PATH_LITERAL("leveldb"));
  base::FileEnumerator file_enum(db_path, true, base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("MANIFEST*"));
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::DeleteFile(name, false);
  }

  // Make sure data is gone.
  context = new LocalStorageContextMojo(
      temp_path(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), test_path, nullptr);
  EXPECT_FALSE(DoTestGet(context, key, &result));

  // Write data again.
  DoTestPut(context, key, value);

  context->ShutdownAndDelete();
  context = nullptr;
  RunUntilIdle();

  // Data should have been preserved now.
  context = new LocalStorageContextMojo(
      temp_path(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), test_path, nullptr);
  EXPECT_TRUE(DoTestGet(context, key, &result));
  EXPECT_EQ(value, result);
  context->ShutdownAndDelete();
}

TEST_F(LocalStorageContextMojoTest, RecreateOnCommitFailure) {
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> test_data;
  base::FilePath test_path(FILE_PATH_LITERAL("test_path"));
  auto* context = new LocalStorageContextMojo(
      temp_path(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), test_path, nullptr);

  FakeLevelDBService mock_leveldb_service;
  context->OverrideLevelDBBinderForTesting(
      base::BindRepeating(&test::FakeLevelDBService::Bind,
                          base::Unretained(&mock_leveldb_service)));

  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  // Open three connections to the database. Two to the same origin, and a third
  // to a different origin.
  mojo::Remote<blink::mojom::StorageArea> area1;
  mojo::Remote<blink::mojom::StorageArea> area2;
  mojo::Remote<blink::mojom::StorageArea> area3;
  {
    base::RunLoop loop;
    mock_leveldb_service.SetOnOpenCallback(loop.QuitClosure());
    context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area1.BindNewPipeAndPassReceiver());
    context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area2.BindNewPipeAndPassReceiver());
    context->OpenLocalStorage(url::Origin::Create(GURL("http://example.com")),
                              area3.BindNewPipeAndPassReceiver());
    loop.Run();
  }

  // Add observers to the first two connections.
  TestLevelDBObserver observer1;
  area1->AddObserver(observer1.Bind());
  TestLevelDBObserver observer2;
  area2->AddObserver(observer2.Bind());

  // Verify one attempt was made to open the database, and connect that request
  // with a database implementation that always fails on write.
  ASSERT_EQ(1u, mock_leveldb_service.open_requests().size());
  auto& open_request = mock_leveldb_service.open_requests()[0];
  auto mock_db = mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<FakeLevelDBDatabaseErrorOnWrite>(&test_data),
      std::move(open_request.receiver));
  std::move(open_request.callback).Run(leveldb::mojom::DatabaseError::OK);
  mock_leveldb_service.open_requests().clear();

  // Setup a RunLoop so we can wait until LocalStorageContextMojo tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  base::RunLoop reopen_loop;
  mock_leveldb_service.SetOnOpenCallback(reopen_loop.QuitClosure());

  // Start a put operation on the third connection before starting to commit
  // a lot of data on the first origin. This put operation should result in a
  // pending commit that will get cancelled when the database connection is
  // closed.
  area3->Put(key, value, base::nullopt, "source",
             base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  size_t values_written = 0;
  while (area1.is_connected()) {
    base::RunLoop put_loop;
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    value[0]++;
    area1.set_disconnect_handler(put_loop.QuitClosure());
    area1->Put(key, value, base::nullopt, "source",
               base::BindLambdaForTesting([&](bool success) {
                 EXPECT_TRUE(success);
               }));
    put_loop.RunUntilIdle();
    values_written++;
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context->FlushOriginForTesting(
        url::Origin::Create(GURL("http://foobar.com")));
  }
  area1.reset();

  // Make sure all messages to the DB have been processed (Flush above merely
  // schedules a commit, but there is no guarantee about those having been
  // processed yet).
  mock_leveldb_service.FlushBindingsForTesting();
  if (mock_db)
    mock_db->FlushForTesting();
  // At this point enough commit failures should have happened to cause the
  // connection to the database to have been severed.
  EXPECT_FALSE(mock_db);

  // The connection to the second area should have closed as well.
  EXPECT_FALSE(area2.is_connected());

  // And the old database should have been destroyed.
  EXPECT_EQ(1u, mock_leveldb_service.destroy_requests().size());

  // Reconnect area1 to the database, and try to read a value.
  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area1.BindNewPipeAndPassReceiver());
  base::RunLoop delete_loop;
  bool success = true;
  TestLevelDBObserver observer3;
  area1->AddObserver(observer3.Bind());
  area1->Delete(key, base::nullopt, "source",
                base::BindLambdaForTesting([&](bool success_in) {
                  success = success_in;
                  delete_loop.Quit();
                }));

  // Wait for LocalStorageContextMojo to try to reconnect to the database, and
  // connect that new request to a properly functioning database.
  reopen_loop.Run();
  ASSERT_EQ(1u, mock_leveldb_service.open_requests().size());
  auto& reopen_request = mock_leveldb_service.open_requests()[0];
  mock_db = mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<FakeLevelDBDatabase>(&test_data),
      std::move(reopen_request.receiver));
  std::move(reopen_request.callback).Run(leveldb::mojom::DatabaseError::OK);
  mock_leveldb_service.open_requests().clear();

  // And deleting the value from the new area should have failed (as the
  // database is empty).
  delete_loop.Run();
  EXPECT_EQ(0u, observer3.observations().size());
  area1.reset();

  {
    // Committing data should now work.
    DoTestPut(context, key, value);
    std::vector<uint8_t> result;
    EXPECT_TRUE(DoTestGet(context, key, &result));
    EXPECT_EQ(value, result);
    EXPECT_FALSE(test_data.empty());
  }

  // Observers should have seen one Add event and a number of Change events for
  // all commits until the connection was closed.
  ASSERT_EQ(values_written, observer2.observations().size());
  for (size_t i = 0; i < values_written; ++i) {
    EXPECT_EQ(i ? TestLevelDBObserver::Observation::kChange
                : TestLevelDBObserver::Observation::kAdd,
              observer2.observations()[i].type);
    EXPECT_EQ(Uint8VectorToStdString(key), observer2.observations()[i].key);
  }
}

TEST_F(LocalStorageContextMojoTest, DontRecreateOnRepeatedCommitFailure) {
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> test_data;

  base::FilePath test_path(FILE_PATH_LITERAL("test_path"));
  auto* context = new LocalStorageContextMojo(
      temp_path(), base::ThreadTaskRunnerHandle::Get(), nullptr,
      base::FilePath(), test_path, nullptr);

  FakeLevelDBService mock_leveldb_service;
  context->OverrideLevelDBBinderForTesting(
      base::BindRepeating(&test::FakeLevelDBService::Bind,
                          base::Unretained(&mock_leveldb_service)));

  auto key = StdStringToUint8Vector("key");
  auto value = StdStringToUint8Vector("value");

  // Open a connection to the database.
  mojo::Remote<blink::mojom::StorageArea> area;
  {
    base::RunLoop loop;
    mock_leveldb_service.SetOnOpenCallback(loop.QuitClosure());
    context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                              area.BindNewPipeAndPassReceiver());
    loop.Run();
  }

  // Verify one attempt was made to open the database, and connect that request
  // with a database implementation that always fails on write.
  ASSERT_EQ(1u, mock_leveldb_service.open_requests().size());
  auto& open_request = mock_leveldb_service.open_requests()[0];
  auto mock_db = mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<FakeLevelDBDatabaseErrorOnWrite>(&test_data),
      std::move(open_request.receiver));
  std::move(open_request.callback).Run(leveldb::mojom::DatabaseError::OK);
  mock_leveldb_service.open_requests().clear();

  // Setup a RunLoop so we can wait until LocalStorageContextMojo tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  base::RunLoop reopen_loop;
  mock_leveldb_service.SetOnOpenCallback(reopen_loop.QuitClosure());

  // Repeatedly write data to the database, to trigger enough commit errors.
  base::Optional<std::vector<uint8_t>> old_value;
  while (area.is_connected()) {
    base::RunLoop put_loop;
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    value[0]++;
    area.set_disconnect_handler(put_loop.QuitClosure());
    area->Put(key, value, old_value, "source",
              base::BindLambdaForTesting([&](bool success) {
                EXPECT_TRUE(success);
                put_loop.Quit();
              }));
    old_value = std::vector<uint8_t>(value);
    put_loop.RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context->FlushOriginForTesting(
        url::Origin::Create(GURL("http://foobar.com")));
  }
  area.reset();

  // Make sure all messages to the DB have been processed (Flush above merely
  // schedules a commit, but there is no guarantee about those having been
  // processed yet).
  if (mock_db)
    mock_db->FlushForTesting();
  // At this point enough commit failures should have happened to cause the
  // connection to the database to have been severed.
  EXPECT_FALSE(mock_db);

  // Wait for LocalStorageContextMojo to try to reconnect to the database, and
  // connect that new request with a database implementation that always fails
  // on write.
  reopen_loop.Run();
  ASSERT_EQ(1u, mock_leveldb_service.open_requests().size());
  auto& reopen_request = mock_leveldb_service.open_requests()[0];
  mock_db = mojo::MakeSelfOwnedAssociatedReceiver(
      std::make_unique<FakeLevelDBDatabaseErrorOnWrite>(&test_data),
      std::move(reopen_request.receiver));
  std::move(reopen_request.callback).Run(leveldb::mojom::DatabaseError::OK);
  mock_leveldb_service.open_requests().clear();

  // The old database should also have been destroyed.
  EXPECT_EQ(1u, mock_leveldb_service.destroy_requests().size());

  // Reconnect a area to the database, and repeatedly write data to it again.
  // This time all should just keep getting written, and commit errors are
  // getting ignored.
  context->OpenLocalStorage(url::Origin::Create(GURL("http://foobar.com")),
                            area.BindNewPipeAndPassReceiver());
  old_value = base::nullopt;
  for (int i = 0; i < 64; ++i) {
    base::RunLoop put_loop;
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    value[0]++;
    area.set_disconnect_handler(put_loop.QuitClosure());
    area->Put(key, value, old_value, "source",
              base::BindLambdaForTesting([&](bool success) {
                EXPECT_TRUE(success);
                put_loop.Quit();
              }));
    put_loop.RunUntilIdle();
    old_value = value;
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context->FlushOriginForTesting(
        url::Origin::Create(GURL("http://foobar.com")));
  }
  // Make sure all messages to the DB have been processed (Flush above merely
  // schedules a commit, but there is no guarantee about those having been
  // processed yet).
  if (mock_db)
    mock_db->FlushForTesting();
  EXPECT_TRUE(mock_db);
  EXPECT_TRUE(area.is_connected());
}

}  // namespace content
