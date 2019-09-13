// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <content/browser/indexed_db/scopes/leveldb_state.h>
#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>

#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "content/browser/indexed_db/leveldb/leveldb_write_batch.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_database.h"
#include "content/browser/indexed_db/scopes/disjoint_range_lock_manager.h"
#include "content/browser/indexed_db/scopes/leveldb_scopes.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"

namespace content {
namespace leveldb_unittest {

static const size_t kDefaultMaxOpenIteratorsPerDatabase = 50;

class SimpleLDBComparator : public leveldb::Comparator {
 public:
  static const SimpleLDBComparator* Get() {
    static const base::NoDestructor<SimpleLDBComparator> simple_ldb_comparator;
    return simple_ldb_comparator.get();
  }
  int Compare(const leveldb::Slice& a, const leveldb::Slice& b) const override {
    size_t len = std::min(a.size(), b.size());
    return memcmp(a.data(), b.data(), len);
  }
  const char* Name() const override { return "temp_comparator"; }
  void FindShortestSeparator(std::string* start,
                             const leveldb::Slice& limit) const override {}
  void FindShortSuccessor(std::string* key) const override {}
};

std::tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /* disk_full*/>
OpenLevelDB(base::FilePath file_name) {
  return indexed_db::LevelDBFactory::Get()->OpenLevelDBState(
      file_name, SimpleLDBComparator::Get(), /* create_if_missing=*/true);
}

class TransactionalLevelDBDatabaseTest : public testing::Test {
 public:
  TransactionalLevelDBDatabaseTest() = default;
  ~TransactionalLevelDBDatabaseTest() override = default;

  leveldb::Status OpenLevelDBDatabase(scoped_refptr<LevelDBState> ldb_state) {
    lock_manager_ = std::make_unique<DisjointRangeLockManager>(1);
    std::unique_ptr<LevelDBScopes> scopes = std::make_unique<LevelDBScopes>(
        std::vector<uint8_t>{'a'}, 1024ul, ldb_state, lock_manager_.get(),
        base::DoNothing());
    leveldb::Status status = scopes->Initialize();
    if (!status.ok())
      return status;
    status = scopes->StartRecoveryAndCleanupTasks(
        LevelDBScopes::TaskRunnerMode::kUseCurrentSequence);
    if (!status.ok())
      return status;
    leveldb_database_ =
        indexed_db::LevelDBFactory::Get()->CreateLevelDBDatabase(
            std::move(ldb_state), std::move(scopes), nullptr,
            kDefaultMaxOpenIteratorsPerDatabase);
    return leveldb::Status::OK();
  }

  base::test::TaskEnvironment task_env_;
  std::unique_ptr<DisjointRangeLockManager> lock_manager_;
  std::unique_ptr<TransactionalLevelDBDatabase> leveldb_database_;
};

TEST_F(TransactionalLevelDBDatabaseTest, CorruptionTest) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  const std::string key("key");
  const std::string value("value");
  std::string put_value;
  std::string got_value;
  scoped_refptr<LevelDBState> ldb_state;
  leveldb::Status status;
  std::tie(ldb_state, status, std::ignore) =
      OpenLevelDB(temp_directory.GetPath());
  EXPECT_TRUE(status.ok());

  status = OpenLevelDBDatabase(ldb_state);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(leveldb_database_);
  put_value = value;
  status = leveldb_database_->Put(key, &put_value);
  EXPECT_TRUE(status.ok());
  leveldb_database_.reset();
  ldb_state.reset();

  std::tie(ldb_state, status, std::ignore) =
      OpenLevelDB(temp_directory.GetPath());
  EXPECT_TRUE(status.ok());

  OpenLevelDBDatabase(ldb_state);
  EXPECT_TRUE(status.ok());
  bool found = false;
  status = leveldb_database_->Get(key, &got_value, &found);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(found);
  EXPECT_EQ(value, got_value);
  leveldb_database_.reset();
  ldb_state.reset();

  EXPECT_TRUE(
      leveldb_chrome::CorruptClosedDBForTesting(temp_directory.GetPath()));

  std::tie(ldb_state, status, std::ignore) =
      OpenLevelDB(temp_directory.GetPath());
  EXPECT_FALSE(status.ok());
  EXPECT_TRUE(status.IsCorruption());

  status = indexed_db::LevelDBFactory::Get()->DestroyLevelDB(
      temp_directory.GetPath());
  EXPECT_TRUE(status.ok());

  std::tie(ldb_state, status, std::ignore) =
      OpenLevelDB(temp_directory.GetPath());
  EXPECT_TRUE(status.ok());

  status = OpenLevelDBDatabase(ldb_state);
  EXPECT_TRUE(status.ok());
  ASSERT_TRUE(leveldb_database_);
  status = leveldb_database_->Get(key, &got_value, &found);
  EXPECT_TRUE(status.ok());
  EXPECT_FALSE(found);
}

TEST(LevelDB, Locking) {
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());

  leveldb::Env* env = LevelDBEnv::Get();
  base::FilePath file = temp_directory.GetPath().AppendASCII("LOCK");
  leveldb::FileLock* lock;
  leveldb::Status status = env->LockFile(file.AsUTF8Unsafe(), &lock);
  EXPECT_TRUE(status.ok());

  status = env->UnlockFile(lock);
  EXPECT_TRUE(status.ok());

  status = env->LockFile(file.AsUTF8Unsafe(), &lock);
  EXPECT_TRUE(status.ok());

  leveldb::FileLock* lock2;
  status = env->LockFile(file.AsUTF8Unsafe(), &lock2);
  EXPECT_FALSE(status.ok());

  status = env->UnlockFile(lock);
  EXPECT_TRUE(status.ok());
}

TEST_F(TransactionalLevelDBDatabaseTest, LastModified) {
  const std::string key("key");
  const std::string value("value");
  std::string put_value;
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  base::SimpleTestClock* clock_ptr = test_clock.get();
  clock_ptr->Advance(base::TimeDelta::FromHours(2));

  leveldb::Status status;
  scoped_refptr<LevelDBState> ldb_state;
  std::tie(ldb_state, status, std::ignore) = OpenLevelDB(base::FilePath());
  EXPECT_TRUE(status.ok());

  status = OpenLevelDBDatabase(ldb_state);
  EXPECT_TRUE(status.ok());
  ASSERT_TRUE(leveldb_database_);
  leveldb_database_->SetClockForTesting(std::move(test_clock));
  // Calling |Put| sets time modified.
  put_value = value;
  base::Time now_time = clock_ptr->Now();
  status = leveldb_database_->Put(key, &put_value);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(now_time, leveldb_database_->LastModified());

  // Calling |Remove| sets time modified.
  clock_ptr->Advance(base::TimeDelta::FromSeconds(200));
  now_time = clock_ptr->Now();
  status = leveldb_database_->Remove(key);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(now_time, leveldb_database_->LastModified());

  // Calling |Write| sets time modified
  clock_ptr->Advance(base::TimeDelta::FromMinutes(15));
  now_time = clock_ptr->Now();
  auto batch = LevelDBWriteBatch::Create();
  batch->Put(key, value);
  batch->Remove(key);
  status = leveldb_database_->Write(batch.get());
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(now_time, leveldb_database_->LastModified());
}

}  // namespace leveldb_unittest
}  // namespace content
