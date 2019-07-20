// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/services/filesystem/public/cpp/manifest.h"
#include "components/services/filesystem/public/mojom/directory.mojom.h"
#include "components/services/filesystem/public/mojom/file_system.mojom.h"
#include "components/services/filesystem/public/mojom/types.mojom.h"
#include "components/services/leveldb/public/cpp/manifest.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "components/services/leveldb/public/mojom/leveldb.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/manifest_builder.h"
#include "services/service_manager/public/cpp/test/test_service.h"
#include "services/service_manager/public/cpp/test/test_service_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/leveldb_features.h"

namespace leveldb {
namespace {

template <typename... Args>
void IgnoreAllArgs(Args&&...) {}

template <typename... Args>
void DoCaptures(typename std::decay<Args>::type*... out_args,
                const base::Closure& quit_closure,
                Args... in_args) {
  IgnoreAllArgs((*out_args = std::move(in_args))...);
  quit_closure.Run();
}

template <typename T1>
base::Callback<void(T1)> Capture(T1* t1, const base::Closure& quit_closure) {
  return base::Bind(&DoCaptures<T1>, t1, quit_closure);
}

template <typename T1, typename T2>
base::Callback<void(T1, T2)> Capture(T1* t1,
                                     T2* t2,
                                     const base::Closure& quit_closure) {
  return base::Bind(&DoCaptures<T1, T2>, t1, t2, quit_closure);
}

template <typename T1>
base::Callback<void(const T1&)> CaptureConstRef(
    T1* t1,
    const base::Closure& quit_closure) {
  return base::Bind(&DoCaptures<const T1&>, t1, quit_closure);
}

template <typename T1, typename T2>
base::Callback<void(T1, const T2&)>
CaptureConstRef(T1* t1, T2* t2, const base::Closure& quit_closure) {
  return base::Bind(&DoCaptures<T1, const T2&>, t1, t2, quit_closure);
}

void DatabaseSyncPut(mojom::LevelDBDatabase* database,
                     const std::string& key,
                     const std::string& value,
                     mojom::DatabaseError* out_error) {
  base::RunLoop run_loop;
  database->Put(StdStringToUint8Vector(key), StdStringToUint8Vector(value),
                Capture(out_error, run_loop.QuitClosure()));
  run_loop.Run();
}

void DatabaseSyncGet(mojom::LevelDBDatabase* database,
                     const std::string& key,
                     mojom::DatabaseError* out_error,
                     std::vector<uint8_t>* out_value) {
  base::RunLoop run_loop;
  database->Get(StdStringToUint8Vector(key),
                CaptureConstRef(out_error, out_value, run_loop.QuitClosure()));
  run_loop.Run();
}

void DatabaseSyncGetPrefixed(mojom::LevelDBDatabase* database,
                             const std::string& key_prefix,
                             mojom::DatabaseError* out_error,
                             std::vector<mojom::KeyValuePtr>* out_key_values) {
  base::RunLoop run_loop;
  database->GetPrefixed(
      StdStringToUint8Vector(key_prefix),
      Capture(out_error, out_key_values, run_loop.QuitClosure()));
  run_loop.Run();
}

void DatabaseSyncGetMany(
    mojom::LevelDBDatabase* database,
    std::vector<mojom::GetManyRequestPtr>& keys_or_prefixes,
    std::vector<mojom::GetManyResultPtr>* out_values) {
  base::RunLoop run_loop;
  database->GetMany(std::move(keys_or_prefixes),
                    Capture(out_values, run_loop.QuitClosure()));
  run_loop.Run();
}

void DatabaseSyncCopyPrefixed(mojom::LevelDBDatabase* database,
                              const std::string& source_key_prefix,
                              const std::string& destination_key_prefix,
                              mojom::DatabaseError* out_error) {
  base::RunLoop run_loop;
  database->CopyPrefixed(StdStringToUint8Vector(source_key_prefix),
                         StdStringToUint8Vector(destination_key_prefix),
                         Capture(out_error, run_loop.QuitClosure()));
  run_loop.Run();
}

void DatabaseSyncDelete(mojom::LevelDBDatabase* database,
                        const std::string& key,
                        mojom::DatabaseError* out_error) {
  base::RunLoop run_loop;
  database->Delete(StdStringToUint8Vector(key),
                   Capture(out_error, run_loop.QuitClosure()));
  run_loop.Run();
}

void DatabaseSyncDeletePrefixed(mojom::LevelDBDatabase* database,
                                const std::string& key_prefix,
                                mojom::DatabaseError* out_error) {
  base::RunLoop run_loop;
  database->DeletePrefixed(StdStringToUint8Vector(key_prefix),
                           Capture(out_error, run_loop.QuitClosure()));
  run_loop.Run();
}

void DatabaseSyncRewrite(mojom::LevelDBDatabase* database,
                         mojom::DatabaseError* out_error) {
  base::RunLoop run_loop;
  database->RewriteDB(Capture(out_error, run_loop.QuitClosure()));
  run_loop.Run();
}

void LevelDBSyncOpenInMemory(mojom::LevelDBService* leveldb,
                             mojom::LevelDBDatabaseAssociatedRequest database,
                             mojom::DatabaseError* out_error) {
  base::RunLoop run_loop;
  leveldb->OpenInMemory(base::nullopt, "LevelDBSync", std::move(database),
                        Capture(out_error, run_loop.QuitClosure()));
  run_loop.Run();
}

void AddKeyToGetManyRequest(const std::string& key,
                            std::vector<mojom::GetManyRequestPtr>* list) {
  std::vector<uint8_t> in_arg = StdStringToUint8Vector(key);
  list->emplace_back(mojom::GetManyRequest::NewKey(in_arg));
}

void AddKeyPrefixToGetManyRequest(const std::string& key_prefix,
                                  std::vector<mojom::GetManyRequestPtr>* list) {
  std::vector<uint8_t> in_arg = StdStringToUint8Vector(key_prefix);
  list->emplace_back(mojom::GetManyRequest::NewKeyPrefix(in_arg));
}

const char kTestServiceName[] = "leveldb_service_unittests";

class LevelDBServiceTest : public testing::Test {
 public:
  LevelDBServiceTest()
      : test_service_manager_(
            {GetManifest(), filesystem::GetManifest(),
             service_manager::ManifestBuilder()
                 .WithServiceName(kTestServiceName)
                 .RequireCapability("filesystem", "filesystem:filesystem")
                 .RequireCapability("leveldb", "leveldb:leveldb")
                 .Build()}),
        test_service_(
            test_service_manager_.RegisterTestInstance(kTestServiceName)) {}
  ~LevelDBServiceTest() override = default;

 protected:
  service_manager::Connector* connector() { return test_service_.connector(); }

  void SetUp() override {
    // TODO(dullweber): This doesn't seem to work. The reason is probably that
    // the LevelDB service is a separate executable here. How should we set
    // features that affect a service?
    feature_list_.InitAndEnableFeature(leveldb::kLevelDBRewriteFeature);
    connector()->BindInterface("filesystem", &files_);
    connector()->BindInterface("leveldb", &leveldb_);
  }

  // Note: This has an out parameter rather than returning the |DirectoryPtr|,
  // since |ASSERT_...()| doesn't work with return values.
  void GetTempDirectory(filesystem::mojom::DirectoryPtr* directory) {
    base::File::Error error = base::File::Error::FILE_ERROR_FAILED;
    bool handled = files()->OpenTempDirectory(MakeRequest(directory), &error);
    ASSERT_TRUE(handled);
    ASSERT_EQ(base::File::Error::FILE_OK, error);
  }

  filesystem::mojom::FileSystemPtr& files() { return files_; }
  mojom::LevelDBServicePtr& leveldb() { return leveldb_; }

 private:
  base::test::ScopedTaskEnvironment task_environment_;
  service_manager::TestServiceManager test_service_manager_;
  service_manager::TestService test_service_;

  base::test::ScopedFeatureList feature_list_;
  filesystem::mojom::FileSystemPtr files_;
  mojom::LevelDBServicePtr leveldb_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBServiceTest);
};

TEST_F(LevelDBServiceTest, Basic) {
  mojom::DatabaseError error;
  mojom::LevelDBDatabaseAssociatedPtr database;
  LevelDBSyncOpenInMemory(leveldb().get(), MakeRequest(&database), &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Write a key to the database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncPut(database.get(), "key", "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Read the key back from the database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  std::vector<uint8_t> value;
  DatabaseSyncGet(database.get(), "key", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("value", Uint8VectorToStdString(value));

  // Delete the key from the database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncDelete(database.get(), "key", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Read the key back from the database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  value.clear();
  DatabaseSyncGet(database.get(), "key", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::NOT_FOUND, error);
  EXPECT_EQ("", Uint8VectorToStdString(value));
}

TEST_F(LevelDBServiceTest, WriteBatch) {
  mojom::DatabaseError error;
  mojom::LevelDBDatabaseAssociatedPtr database;
  LevelDBSyncOpenInMemory(leveldb().get(), MakeRequest(&database), &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Write a key to the database.
  DatabaseSyncPut(database.get(), "key", "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Create a batched operation which both deletes "key" and adds another write.
  std::vector<mojom::BatchedOperationPtr> operations;
  mojom::BatchedOperationPtr item = mojom::BatchedOperation::New();
  item->type = mojom::BatchOperationType::DELETE_KEY;
  item->key = StdStringToUint8Vector("key");
  operations.push_back(std::move(item));

  item = mojom::BatchedOperation::New();
  item->type = mojom::BatchOperationType::PUT_KEY;
  item->key = StdStringToUint8Vector("other");
  item->value = StdStringToUint8Vector("more");
  operations.push_back(std::move(item));

  base::RunLoop run_loop;
  database->Write(std::move(operations),
                  Capture(&error, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Reading "key" should be invalid now.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  std::vector<uint8_t> value;
  DatabaseSyncGet(database.get(), "key", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::NOT_FOUND, error);
  EXPECT_EQ("", Uint8VectorToStdString(value));

  // Reading "other" should return "more"
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncGet(database.get(), "other", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("more", Uint8VectorToStdString(value));

  // Write a some prefixed keys to the database.
  DatabaseSyncPut(database.get(), "prefix-key1", "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  DatabaseSyncPut(database.get(), "prefix-key2", "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Create batched operations to copy and then delete the 'prefix' data.
  operations.clear();
  item = mojom::BatchedOperation::New();
  item->type = mojom::BatchOperationType::COPY_PREFIXED_KEY;
  item->key = StdStringToUint8Vector("prefix");
  item->value = StdStringToUint8Vector("copy-prefix");
  operations.push_back(std::move(item));
  item = mojom::BatchedOperation::New();
  item->type = mojom::BatchOperationType::DELETE_PREFIXED_KEY;
  item->key = StdStringToUint8Vector("prefix");
  operations.push_back(std::move(item));
  base::RunLoop run_loop2;
  database->Write(std::move(operations),
                  Capture(&error, run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Reading all "prefix" keys should be invalid now.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  value.clear();
  DatabaseSyncGet(database.get(), "prefix-key1", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::NOT_FOUND, error);
  EXPECT_EQ("", Uint8VectorToStdString(value));
  // Reading "key" should be invalid now.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  value.clear();
  DatabaseSyncGet(database.get(), "prefix-key2", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::NOT_FOUND, error);
  EXPECT_EQ("", Uint8VectorToStdString(value));

  // Prefix keys should have been copied to 'copy-prefix' before deletion.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncGet(database.get(), "copy-prefix-key1", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("value", Uint8VectorToStdString(value));
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncGet(database.get(), "copy-prefix-key2", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("value", Uint8VectorToStdString(value));
}

TEST_F(LevelDBServiceTest, WriteBatchPrefixesAndDeletes) {
  // This test makes sure that prefixes & deletes happen before all other batch
  // operations.
  mojom::DatabaseError error;
  mojom::LevelDBDatabaseAssociatedPtr database;
  LevelDBSyncOpenInMemory(leveldb().get(), MakeRequest(&database), &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Write a key to the database.
  DatabaseSyncPut(database.get(), "key", "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // The copy applies as if it happens before this write batch.
  // The delete applies to all keys that existed before these changes.
  std::vector<mojom::BatchedOperationPtr> operations;
  mojom::BatchedOperationPtr item = mojom::BatchedOperation::New();
  item->type = mojom::BatchOperationType::PUT_KEY;
  item->key = StdStringToUint8Vector("key");
  item->value = StdStringToUint8Vector("new_value");
  operations.push_back(std::move(item));

  item = mojom::BatchedOperation::New();
  item->type = mojom::BatchOperationType::PUT_KEY;
  item->key = StdStringToUint8Vector("key2");
  item->value = StdStringToUint8Vector("value2");
  operations.push_back(std::move(item));

  item = mojom::BatchedOperation::New();
  item->type = mojom::BatchOperationType::DELETE_PREFIXED_KEY;
  item->key = StdStringToUint8Vector("k");
  operations.push_back(std::move(item));

  item = mojom::BatchedOperation::New();
  item->type = mojom::BatchOperationType::COPY_PREFIXED_KEY;
  item->key = StdStringToUint8Vector("k");
  item->value = StdStringToUint8Vector("f");
  operations.push_back(std::move(item));
  base::RunLoop run_loop;
  database->Write(std::move(operations),
                  Capture(&error, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  std::vector<uint8_t> value;
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncGet(database.get(), "key", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::NOT_FOUND, error);
  DatabaseSyncGet(database.get(), "key2", &error, &value);
  EXPECT_EQ("value2", Uint8VectorToStdString(value));
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncGet(database.get(), "fey", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("value", Uint8VectorToStdString(value));
}

TEST_F(LevelDBServiceTest, Reconnect) {
  mojom::DatabaseError error;

  filesystem::mojom::DirectoryPtr temp_directory;
  GetTempDirectory(&temp_directory);

  {
    filesystem::mojom::DirectoryPtr directory;
    temp_directory->Clone(MakeRequest(&directory));

    mojom::LevelDBDatabaseAssociatedPtr database;
    leveldb_env::Options options;
    options.error_if_exists = true;
    options.create_if_missing = true;
    base::RunLoop run_loop;
    leveldb()->OpenWithOptions(std::move(options), std::move(directory), "test",
                               base::nullopt, MakeRequest(&database),
                               Capture(&error, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(mojom::DatabaseError::OK, error);

    // Write a key to the database.
    error = mojom::DatabaseError::INVALID_ARGUMENT;
    DatabaseSyncPut(database.get(), "key", "value", &error);
    EXPECT_EQ(mojom::DatabaseError::OK, error);

    // The database should go out of scope here.
  }

  {
    filesystem::mojom::DirectoryPtr directory;
    temp_directory->Clone(MakeRequest(&directory));

    // Reconnect to the database.
    mojom::LevelDBDatabaseAssociatedPtr database;
    base::RunLoop run_loop;
    leveldb()->Open(std::move(directory), "test", base::nullopt,
                    MakeRequest(&database),
                    Capture(&error, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(mojom::DatabaseError::OK, error);

    // We should still be able to read the key back from the database.
    error = mojom::DatabaseError::INVALID_ARGUMENT;
    std::vector<uint8_t> value;
    DatabaseSyncGet(database.get(), "key", &error, &value);
    EXPECT_EQ(mojom::DatabaseError::OK, error);
    EXPECT_EQ("value", Uint8VectorToStdString(value));
  }
}

TEST_F(LevelDBServiceTest, Destroy) {
  mojom::DatabaseError error;

  filesystem::mojom::DirectoryPtr temp_directory;
  GetTempDirectory(&temp_directory);

  {
    filesystem::mojom::DirectoryPtr directory;
    temp_directory->Clone(MakeRequest(&directory));

    mojom::LevelDBDatabaseAssociatedPtr database;
    leveldb_env::Options options;
    options.error_if_exists = true;
    options.create_if_missing = true;
    base::RunLoop run_loop;
    leveldb()->OpenWithOptions(std::move(options), std::move(directory), "test",
                               base::nullopt, MakeRequest(&database),
                               Capture(&error, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(mojom::DatabaseError::OK, error);

    // Write a key to the database.
    error = mojom::DatabaseError::INVALID_ARGUMENT;
    DatabaseSyncPut(database.get(), "key", "value", &error);
    EXPECT_EQ(mojom::DatabaseError::OK, error);

    // The database should go out of scope here.
  }

  {
    filesystem::mojom::DirectoryPtr directory;
    temp_directory->Clone(MakeRequest(&directory));

    // Destroy the database.
    base::RunLoop run_loop;
    leveldb()->Destroy(std::move(directory), "test",
                       Capture(&error, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(mojom::DatabaseError::OK, error);
  }

  {
    filesystem::mojom::DirectoryPtr directory;
    temp_directory->Clone(MakeRequest(&directory));

    // Reconnect to the database should fail.
    mojom::LevelDBDatabaseAssociatedPtr database;
    base::RunLoop run_loop;
    leveldb()->Open(std::move(directory), "test", base::nullopt,
                    MakeRequest(&database),
                    Capture(&error, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(mojom::DatabaseError::INVALID_ARGUMENT, error);
  }

  {
    filesystem::mojom::DirectoryPtr directory;
    temp_directory->Clone(MakeRequest(&directory));

    // Destroying a non-existant database should still succeed.
    base::RunLoop run_loop;
    leveldb()->Destroy(std::move(directory), "test",
                       Capture(&error, run_loop.QuitClosure()));
    run_loop.Run();
    EXPECT_EQ(mojom::DatabaseError::OK, error);
  }
}

TEST_F(LevelDBServiceTest, GetSnapshotSimple) {
  mojom::DatabaseError error;
  mojom::LevelDBDatabaseAssociatedPtr database;
  LevelDBSyncOpenInMemory(leveldb().get(), MakeRequest(&database), &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  base::UnguessableToken snapshot;
  base::RunLoop run_loop;
  database->GetSnapshot(CaptureConstRef(&snapshot, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_FALSE(snapshot.is_empty());
}

TEST_F(LevelDBServiceTest, GetFromSnapshots) {
  mojom::DatabaseError error;
  mojom::LevelDBDatabaseAssociatedPtr database;
  LevelDBSyncOpenInMemory(leveldb().get(), MakeRequest(&database), &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Write a key to the database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncPut(database.get(), "key", "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Take a snapshot where key=value.
  base::UnguessableToken key_value_snapshot;
  base::RunLoop run_loop;
  database->GetSnapshot(
      CaptureConstRef(&key_value_snapshot, run_loop.QuitClosure()));
  run_loop.Run();

  // Change key to "yek".
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncPut(database.get(), "key", "yek", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // (Ensure this change is live on the database.)
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  std::vector<uint8_t> value;
  DatabaseSyncGet(database.get(), "key", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("yek", Uint8VectorToStdString(value));

  // But if we were to read from the snapshot, we'd still get value.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  value.clear();
  base::RunLoop run_loop2;
  database->GetFromSnapshot(
      key_value_snapshot, StdStringToUint8Vector("key"),
      CaptureConstRef(&error, &value, run_loop2.QuitClosure()));
  run_loop2.Run();
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("value", Uint8VectorToStdString(value));
}

TEST_F(LevelDBServiceTest, InvalidArgumentOnInvalidSnapshot) {
  mojom::LevelDBDatabaseAssociatedPtr database;
  mojom::DatabaseError error = mojom::DatabaseError::INVALID_ARGUMENT;
  LevelDBSyncOpenInMemory(leveldb().get(), MakeRequest(&database), &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  base::UnguessableToken invalid_snapshot = base::UnguessableToken::Create();

  error = mojom::DatabaseError::OK;
  std::vector<uint8_t> value;
  base::RunLoop run_loop;
  database->GetFromSnapshot(
      invalid_snapshot, StdStringToUint8Vector("key"),
      CaptureConstRef(&error, &value, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(mojom::DatabaseError::INVALID_ARGUMENT, error);
}

TEST_F(LevelDBServiceTest, MemoryDBReadWrite) {
  mojom::LevelDBDatabaseAssociatedPtr database;
  mojom::DatabaseError error = mojom::DatabaseError::INVALID_ARGUMENT;
  LevelDBSyncOpenInMemory(leveldb().get(), MakeRequest(&database), &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Write a key to the database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncPut(database.get(), "key", "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Read the key back from the database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  std::vector<uint8_t> value;
  DatabaseSyncGet(database.get(), "key", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("value", Uint8VectorToStdString(value));

  // Delete the key from the database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncDelete(database.get(), "key", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Read the key back from the database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  value.clear();
  DatabaseSyncGet(database.get(), "key", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::NOT_FOUND, error);
  EXPECT_EQ("", Uint8VectorToStdString(value));
}

TEST_F(LevelDBServiceTest, Prefixed) {
  // Open an in memory database for speed.
  mojom::DatabaseError error = mojom::DatabaseError::INVALID_ARGUMENT;
  mojom::LevelDBDatabaseAssociatedPtr database;
  LevelDBSyncOpenInMemory(leveldb().get(), MakeRequest(&database), &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  const std::string prefix("prefix");
  const std::string copy_prefix("foo");
  std::vector<mojom::KeyValuePtr> key_values;

  // Completely empty database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncGetPrefixed(database.get(), prefix, &error, &key_values);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_TRUE(key_values.empty());

  // No values with our prefix, but values before and after.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncPut(database.get(), "a-before-prefix", "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncPut(database.get(), "z-after-prefix", "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  key_values.clear();
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncGetPrefixed(database.get(), prefix, &error, &key_values);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_TRUE(key_values.empty());

  // One value with the exact prefix.
  DatabaseSyncPut(database.get(), prefix, "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  key_values.clear();
  DatabaseSyncGetPrefixed(database.get(), prefix, &error, &key_values);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ(1u, key_values.size());
  EXPECT_EQ("prefix", Uint8VectorToStdString(key_values[0]->key));
  EXPECT_EQ("value", Uint8VectorToStdString(key_values[0]->value));

  // Multiple values with starting with the prefix.
  DatabaseSyncPut(database.get(), (prefix + "2"), "value2", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  key_values.clear();
  DatabaseSyncGetPrefixed(database.get(), prefix, &error, &key_values);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ(2u, key_values.size());
  EXPECT_EQ("prefix", Uint8VectorToStdString(key_values[0]->key));
  EXPECT_EQ("value", Uint8VectorToStdString(key_values[0]->value));
  EXPECT_EQ("prefix2", Uint8VectorToStdString(key_values[1]->key));
  EXPECT_EQ("value2", Uint8VectorToStdString(key_values[1]->value));

  // Copy to a different prefix
  DatabaseSyncCopyPrefixed(database.get(), prefix, copy_prefix, &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  DatabaseSyncGetPrefixed(database.get(), copy_prefix, &error, &key_values);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  ASSERT_EQ(2u, key_values.size());
  EXPECT_EQ("foo", Uint8VectorToStdString(key_values[0]->key));
  EXPECT_EQ("value", Uint8VectorToStdString(key_values[0]->value));
  EXPECT_EQ("foo2", Uint8VectorToStdString(key_values[1]->key));
  EXPECT_EQ("value2", Uint8VectorToStdString(key_values[1]->value));

  // Delete the prefixed values.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncDeletePrefixed(database.get(), prefix, &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  key_values.clear();
  DatabaseSyncGetPrefixed(database.get(), prefix, &error, &key_values);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_TRUE(key_values.empty());

  // Make sure the others are not deleted.
  std::vector<uint8_t> value;
  DatabaseSyncGet(database.get(), "a-before-prefix", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("value", Uint8VectorToStdString(value));
  value.clear();
  DatabaseSyncGet(database.get(), "z-after-prefix", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("value", Uint8VectorToStdString(value));
  DatabaseSyncGetPrefixed(database.get(), copy_prefix, &error, &key_values);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ(2u, key_values.size());

  // A key having our prefix, but no key matching it exactly.
  // Even thought there is no exact matching key, GetPrefixed
  // and DeletePrefixed still operate on the values.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncPut(database.get(), (prefix + "2"), "value2", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  key_values.clear();
  DatabaseSyncGetPrefixed(database.get(), prefix, &error, &key_values);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ(1u, key_values.size());
  EXPECT_EQ("prefix2", Uint8VectorToStdString(key_values[0]->key));
  EXPECT_EQ("value2", Uint8VectorToStdString(key_values[0]->value));
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncDeletePrefixed(database.get(), prefix, &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  key_values.clear();
  DatabaseSyncGetPrefixed(database.get(), prefix, &error, &key_values);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_TRUE(key_values.empty());
}

TEST_F(LevelDBServiceTest, RewriteDB) {
  mojom::DatabaseError error;
  filesystem::mojom::DirectoryPtr directory;
  GetTempDirectory(&directory);

  mojom::LevelDBDatabaseAssociatedPtr database;
  leveldb_env::Options options;
  options.create_if_missing = true;
  base::RunLoop run_loop;
  leveldb()->OpenWithOptions(std::move(options), std::move(directory), "test",
                             base::nullopt, MakeRequest(&database),
                             Capture(&error, run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Write entries to the database.
  DatabaseSyncPut(database.get(), "key1", "value1", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  DatabaseSyncPut(database.get(), "key2", "value2", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Delete key 1.
  DatabaseSyncDelete(database.get(), "key1", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Perform a rewrite.
  DatabaseSyncRewrite(database.get(), &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Read the keys back from the database.
  std::vector<uint8_t> value;
  DatabaseSyncGet(database.get(), "key1", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::NOT_FOUND, error);
  DatabaseSyncGet(database.get(), "key2", &error, &value);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  EXPECT_EQ("value2", Uint8VectorToStdString(value));
}

TEST_F(LevelDBServiceTest, GetMany) {
  mojom::DatabaseError error;
  mojom::LevelDBDatabaseAssociatedPtr database;
  LevelDBSyncOpenInMemory(leveldb().get(), MakeRequest(&database), &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  // Write two keys to the database.
  error = mojom::DatabaseError::INVALID_ARGUMENT;
  DatabaseSyncPut(database.get(), "key", "value", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);
  DatabaseSyncPut(database.get(), "key1", "value1", &error);
  EXPECT_EQ(mojom::DatabaseError::OK, error);

  std::vector<mojom::GetManyRequestPtr> requests;
  std::vector<mojom::GetManyResultPtr> results;

  // Test: Read keys back from the database
  AddKeyToGetManyRequest("key", &requests);
  AddKeyToGetManyRequest("key1", &requests);
  AddKeyPrefixToGetManyRequest("key", &requests);
  DatabaseSyncGetMany(database.get(), requests, &results);

  ASSERT_EQ(results.size(), 3UL);
  EXPECT_TRUE(results[0]->is_key_value());
  EXPECT_TRUE(results[1]->is_key_value());
  EXPECT_EQ(Uint8VectorToStdString(results[0]->get_key_value()), "value");
  EXPECT_EQ(Uint8VectorToStdString(results[1]->get_key_value()), "value1");

  // Test: the last key-prefix read should correctly return a list of KeyValue
  EXPECT_TRUE(results[2]->is_key_prefix_values());
  const std::vector<mojom::KeyValuePtr>& kv =
      results[2]->get_key_prefix_values();
  ASSERT_EQ(2UL, kv.size());
  EXPECT_EQ("key", Uint8VectorToStdString(kv[0]->key));
  EXPECT_EQ("value", Uint8VectorToStdString(kv[0]->value));
  EXPECT_EQ("key1", Uint8VectorToStdString(kv[1]->key));
  EXPECT_EQ("value1", Uint8VectorToStdString(kv[1]->value));

  // Test: Read a sequence of (existing key, non-existing key,
  // existing key), GetMany should return NOT_FOUND for the non-existing key
  requests.clear();
  results.clear();

  AddKeyToGetManyRequest("key", &requests);
  AddKeyToGetManyRequest("key-not-found", &requests);
  AddKeyToGetManyRequest("key", &requests);
  DatabaseSyncGetMany(database.get(), requests, &results);

  ASSERT_EQ(results.size(), 3UL);
  EXPECT_EQ(Uint8VectorToStdString(results[0]->get_key_value()), "value");
  EXPECT_TRUE(results[1]->is_status());
  EXPECT_TRUE(results[1]->get_status() == mojom::DatabaseError::NOT_FOUND);
  EXPECT_EQ(Uint8VectorToStdString(results[2]->get_key_value()), "value");

  // Test: Read a sequence of (existing key, non-existing key prefix,
  // existing key), GetMany should return empty data for the non-existing
  // key prefix
  requests.clear();
  results.clear();

  AddKeyToGetManyRequest("key", &requests);
  AddKeyPrefixToGetManyRequest("key-prefix-not-found", &requests);
  AddKeyToGetManyRequest("key", &requests);
  DatabaseSyncGetMany(database.get(), requests, &results);

  ASSERT_EQ(results.size(), 3UL);
  EXPECT_EQ(Uint8VectorToStdString(results[0]->get_key_value()), "value");
  EXPECT_TRUE(results[1]->get_key_prefix_values().empty());
  EXPECT_EQ(Uint8VectorToStdString(results[2]->get_key_value()), "value");

  // Test: Read empty data
  requests.clear();
  results.clear();
  DatabaseSyncGetMany(database.get(), requests, &results);
  EXPECT_TRUE(results.empty());
}

}  // namespace
}  // namespace leveldb
