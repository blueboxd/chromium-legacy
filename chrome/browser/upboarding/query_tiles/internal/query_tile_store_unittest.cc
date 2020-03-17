// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/query_tile_store.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/test/task_environment.h"
#include "chrome/browser/upboarding/query_tiles/internal/proto_conversion.h"
#include "components/leveldb_proto/public/proto_database.h"
#include "components/leveldb_proto/testing/fake_db.h"
#include "testing/gtest/include/gtest/gtest.h"

using leveldb_proto::test::FakeDB;
using InitStatus = leveldb_proto::Enums::InitStatus;

namespace upboarding {
namespace {

class QueryTileStoreTest : public testing::Test {
 public:
  using QueryTileEntryProto = query_tiles::proto::QueryTileEntry;
  using EntriesMap = std::map<std::string, std::unique_ptr<QueryTileEntry>>;
  using ProtoMap = std::map<std::string, QueryTileEntryProto>;
  using KeysAndEntries = std::map<std::string, QueryTileEntry>;
  using TestEntries = std::vector<QueryTileEntry>;

  QueryTileStoreTest() : load_result_(false), db_(nullptr) {}
  ~QueryTileStoreTest() override = default;

  void SetUp() override {}

  QueryTileStoreTest(const QueryTileStoreTest& other) = delete;
  QueryTileStoreTest& operator=(const QueryTileStoreTest& other) = delete;

 protected:
  void Init(TestEntries input, InitStatus status) {
    CreateTestDbEntries(std::move(input));
    auto db = std::make_unique<FakeDB<QueryTileEntryProto, QueryTileEntry>>(
        &db_entries_);
    db_ = db.get();
    store_ = std::make_unique<QueryTileStore>(std::move(db));
    store_->InitAndLoad(base::BindOnce(&QueryTileStoreTest::OnEntriesLoaded,
                                       base::Unretained(this)));
    db_->InitStatusCallback(status);
  }

  void OnEntriesLoaded(bool success, EntriesMap loaded_entries) {
    load_result_ = success;
    in_memory_entries_ = std::move(loaded_entries);
  }

  void CreateTestDbEntries(TestEntries input) {
    for (auto entry : input) {
      QueryTileEntryProto proto;
      upboarding::QueryTileEntryToProto(&entry, &proto);
      db_entries_.emplace(entry.id, proto);
    }
  }

  // Verifies the entries in the db is |expected|.
  void VerifyDataInDb(std::unique_ptr<KeysAndEntries> expected) {
    db_->LoadKeysAndEntries(
        base::BindOnce(&QueryTileStoreTest::OnVerifyDataInDb,
                       base::Unretained(this), std::move(expected)));
    db_->LoadCallback(true);
  }

  void OnVerifyDataInDb(std::unique_ptr<KeysAndEntries> expected,
                        bool success,
                        std::unique_ptr<KeysAndEntries> loaded_entries) {
    EXPECT_TRUE(success);
    DCHECK(expected);
    DCHECK(loaded_entries);
    EXPECT_EQ(*expected.get(), *loaded_entries.get());
  }

  bool load_result() const { return load_result_; }
  const EntriesMap& in_memory_entries() const { return in_memory_entries_; }
  FakeDB<QueryTileEntryProto, QueryTileEntry>* db() { return db_; }
  Store<QueryTileEntry>* store() { return store_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;
  bool load_result_;
  EntriesMap in_memory_entries_;
  ProtoMap db_entries_;
  FakeDB<QueryTileEntryProto, QueryTileEntry>* db_;
  std::unique_ptr<Store<QueryTileEntry>> store_;
};

// Test Initializing and loading an empty database .
TEST_F(QueryTileStoreTest, InitSuccessEmptyDb) {
  auto test_data = TestEntries();
  Init(test_data, InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(in_memory_entries().empty());
}

// Test Initializing and loading a non-empty database.
TEST_F(QueryTileStoreTest, InitSuccessWithData) {
  auto test_data = TestEntries();
  auto test_entry = QueryTileEntry();
  test_entry.id = "test-id";
  test_data.emplace_back(test_entry);
  Init(test_data, InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_EQ(in_memory_entries().size(), 1u);
  auto actual = in_memory_entries().begin();
  EXPECT_EQ(actual->first, test_entry.id);
  EXPECT_EQ(*(actual->second.get()), test_entry);
}

// Test Initializing and loading a non-empty database failed.
TEST_F(QueryTileStoreTest, InitFailedWithData) {
  auto test_data = TestEntries();
  auto test_entry = QueryTileEntry();
  test_entry.id = "test-id";
  test_data.emplace_back(test_entry);
  Init(test_data, InitStatus::kOK);
  db()->LoadCallback(false);
  EXPECT_EQ(load_result(), false);
  EXPECT_TRUE(in_memory_entries().empty());
}

// Test adding and updating.
TEST_F(QueryTileStoreTest, AddAndUpdateDataSuccess) {
  auto test_data = TestEntries();
  Init(test_data, InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_TRUE(in_memory_entries().empty());

  // Add an entry failed.
  auto test_entry_1 = QueryTileEntry();
  test_entry_1.id = "test_entry_id_1";
  test_entry_1.display_text = "test_entry_test_display_text";
  store()->Update(test_entry_1.id, test_entry_1,
                  base::BindOnce([](bool success) { EXPECT_FALSE(success); }));
  db()->UpdateCallback(false);

  // Add an entry successfully.
  store()->Update(test_entry_1.id, test_entry_1,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  auto expected = std::make_unique<KeysAndEntries>();
  expected->emplace(test_entry_1.id, test_entry_1);
  VerifyDataInDb(std::move(expected));

  // Update an existing entry successfully.
  test_entry_1.display_text = "test_entry_test_display_text_updated";
  store()->Update(test_entry_1.id, test_entry_1,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);

  expected = std::make_unique<KeysAndEntries>();
  expected->emplace(test_entry_1.id, test_entry_1);
  VerifyDataInDb(std::move(expected));
}

// Test Deleting from db.
TEST_F(QueryTileStoreTest, DeleteSuccess) {
  auto test_data = TestEntries();
  auto test_entry = QueryTileEntry();
  test_entry.id = "test-id";
  test_data.emplace_back(test_entry);
  Init(test_data, InitStatus::kOK);
  db()->LoadCallback(true);
  EXPECT_EQ(load_result(), true);
  EXPECT_EQ(in_memory_entries().size(), 1u);
  auto actual = in_memory_entries().begin();
  EXPECT_EQ(actual->first, test_entry.id);
  EXPECT_EQ(*(actual->second.get()), test_entry);

  store()->Delete(test_entry.id,
                  base::BindOnce([](bool success) { EXPECT_TRUE(success); }));
  db()->UpdateCallback(true);
  // No entry is expected in db.
  auto expected = std::make_unique<KeysAndEntries>();
  VerifyDataInDb(std::move(expected));
}

}  // namespace
}  // namespace upboarding
