// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/icon_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chrome/browser/notifications/proto/icon.pb.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifications {
namespace {

const char kEntryId[] = "proto_id_1";
const char kEntryId2[] = "proto_id_2";
const char kEntryData[] = "data_1";
const char kEntryData2[] = "data_2";

class IconStoreTest : public testing::Test {
 public:
  IconStoreTest() : load_result_(false), db_(nullptr) {}
  ~IconStoreTest() override = default;

  void SetUp() override {
    IconEntry entry;
    entry.uuid = kEntryId;
    entry.data = kEntryData;
    proto::Icon proto;
    leveldb_proto::DataToProto(&entry, &proto);
    db_entries_.emplace(kEntryId, proto);

    auto db =
        std::make_unique<leveldb_proto::test::FakeDB<proto::Icon, IconEntry>>(
            &db_entries_);
    db_ = db.get();
    store_ = std::make_unique<IconProtoDbStore>(std::move(db));
  }

  void InitDb() {
    store()->Init(base::DoNothing());
    db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  }

  void LoadIcons(std::vector<std::string> keys) {
    store()->LoadIcons(keys, base::BindOnce(&IconStoreTest::OnEntriesLoaded,
                                            base::Unretained(this)));
  }

  void OnEntriesLoaded(bool success,
                       std::unique_ptr<std::vector<IconEntry>> entries) {
    loaded_entries_ = std::move(entries);
    load_result_ = success;
  }

 protected:
  IconStore* store() { return store_.get(); }
  leveldb_proto::test::FakeDB<proto::Icon, IconEntry>* db() { return db_; }
  bool load_result() const { return load_result_; }
  const std::vector<IconEntry>* loaded_entries() {
    return loaded_entries_.get();
  }

  void VerifyEntries(std::vector<std::pair<std::string, std::string>> inputs) {
    EXPECT_EQ(inputs.size(), loaded_entries_->size());
    for (size_t i = 0; i < inputs.size(); i++) {
      EXPECT_EQ(loaded_entries_->at(i).uuid, inputs[i].first);
      EXPECT_EQ(loaded_entries_->at(i).data, inputs[i].second);
    }
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<IconStore> store_;
  std::map<std::string, proto::Icon> db_entries_;
  std::unique_ptr<std::vector<IconEntry>> loaded_entries_;
  bool load_result_;
  leveldb_proto::test::FakeDB<proto::Icon, IconEntry>* db_;

  DISALLOW_COPY_AND_ASSIGN(IconStoreTest);
};

TEST_F(IconStoreTest, Init) {
  store()->Init(base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kOK);
  base::RunLoop().RunUntilIdle();
}

TEST_F(IconStoreTest, InitFailed) {
  store()->Init(base::BindOnce([](bool success) { EXPECT_FALSE(success); }));
  db()->InitStatusCallback(leveldb_proto::Enums::InitStatus::kCorrupt);
  base::RunLoop().RunUntilIdle();
}

TEST_F(IconStoreTest, LoadOne) {
  InitDb();
  LoadIcons({kEntryId});
  db()->LoadCallback(true);

  // Verify data is loaded.
  DCHECK(loaded_entries());
  EXPECT_TRUE(load_result());
  VerifyEntries({{kEntryId, kEntryData}});
}

TEST_F(IconStoreTest, LoadFailed) {
  InitDb();
  LoadIcons({kEntryId});
  db()->LoadCallback(false);

  // Verify load failure.
  EXPECT_FALSE(loaded_entries());
  EXPECT_FALSE(load_result());
}

TEST_F(IconStoreTest, Add) {
  InitDb();

  IconEntry new_entry;
  new_entry.uuid = kEntryId2;
  new_entry.data = kEntryData2;
  store()->Add(std::move(new_entry), base::DoNothing());
  db()->UpdateCallback(true);

  // Verify the entry is added.
  LoadIcons({kEntryId2});
  db()->LoadCallback(true);
  EXPECT_TRUE(load_result());
  VerifyEntries({{kEntryId2, kEntryData2}});
}

TEST_F(IconStoreTest, AddDuplicate) {
  InitDb();

  IconEntry new_entry;
  new_entry.uuid = kEntryId;
  new_entry.data = kEntryData2;
  store()->Add(std::move(new_entry), base::DoNothing());
  db()->UpdateCallback(true);

  // Add a duplicate id is currently allowed, we just update the entry.
  LoadIcons({kEntryId});
  db()->LoadCallback(true);
  EXPECT_TRUE(load_result());
  VerifyEntries({{kEntryId, kEntryData2}});
}

TEST_F(IconStoreTest, Delete) {
  InitDb();

  // Delete the only entry.
  store()->Delete(kEntryId,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  // No entry can be loaded, move nullptr as result.
  LoadIcons({kEntryId});
  db()->LoadCallback(true);

  EXPECT_TRUE(load_result());
  VerifyEntries({});
}

TEST_F(IconStoreTest, DeleteIcons) {
  InitDb();

  // Add one extra entry first.
  IconEntry new_entry;
  new_entry.uuid = kEntryId2;
  new_entry.data = kEntryData2;
  store()->Add(std::move(new_entry), base::DoNothing());
  db()->UpdateCallback(true);

  std::vector<std::string> keys = {kEntryId, kEntryId2};
  store()->DeleteIcons(
      keys, base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  LoadIcons(keys);
  db()->LoadCallback(true);

  // Verify no entries are loaded.
  EXPECT_TRUE(load_result());
  VerifyEntries({});
}

TEST_F(IconStoreTest, AddAndLoadIcons) {
  InitDb();

  std::vector<IconEntry> input(2);
  input[0].uuid = kEntryId;
  input[0].data = kEntryData;
  input[1].uuid = kEntryId2;
  input[1].data = kEntryData2;

  store()->AddIcons(std::move(input), base::DoNothing());
  db()->UpdateCallback(true);

  std::vector<std::string> keys = {kEntryId, kEntryId2};
  LoadIcons(keys);
  db()->LoadCallback(true);

  // Verify entries loaded correctly.
  EXPECT_TRUE(load_result());
  std::vector<std::pair<std::string, std::string>> inputs = {
      {std::move(kEntryId), std::move(kEntryData)},
      {std::move(kEntryId2), std::move(kEntryData2)}};
  VerifyEntries(std::move(inputs));
}

}  // namespace
}  // namespace notifications
