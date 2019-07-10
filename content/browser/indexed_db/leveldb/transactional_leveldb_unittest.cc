// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "base/test/simple_test_clock.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "content/browser/indexed_db/leveldb/leveldb_write_batch.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_database.h"
#include "content/browser/indexed_db/scopes/leveldb_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"

namespace content {
namespace leveldb_unittest {

static const size_t kDefaultMaxOpenIteratorsPerDatabase = 50;

class SimpleComparator : public LevelDBComparator {
 public:
  static const SimpleComparator* Get() {
    static const base::NoDestructor<SimpleComparator> simple_comparator;
    return simple_comparator.get();
  }
  int Compare(const base::StringPiece& a,
              const base::StringPiece& b) const override {
    size_t len = std::min(a.size(), b.size());
    return memcmp(a.begin(), b.begin(), len);
  }
  const char* Name() const override { return "temp_comparator"; }
};

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
      file_name, SimpleComparator::Get(), SimpleLDBComparator::Get());
}

TEST(LevelDBDatabaseTest, CorruptionTest) {
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

  std::unique_ptr<TransactionalLevelDBDatabase> leveldb =
      std::make_unique<TransactionalLevelDBDatabase>(
          std::move(ldb_state), nullptr, nullptr,
          kDefaultMaxOpenIteratorsPerDatabase);
  EXPECT_TRUE(leveldb);
  put_value = value;
  status = leveldb->Put(key, &put_value);
  EXPECT_TRUE(status.ok());
  leveldb.reset();
  EXPECT_FALSE(leveldb);

  std::tie(ldb_state, status, std::ignore) =
      OpenLevelDB(temp_directory.GetPath());
  EXPECT_TRUE(status.ok());
  leveldb = std::make_unique<TransactionalLevelDBDatabase>(
      std::move(ldb_state), nullptr, nullptr,
      kDefaultMaxOpenIteratorsPerDatabase);
  EXPECT_TRUE(leveldb);
  bool found = false;
  status = leveldb->Get(key, &got_value, &found);
  EXPECT_TRUE(status.ok());
  EXPECT_TRUE(found);
  EXPECT_EQ(value, got_value);
  leveldb.reset();
  EXPECT_FALSE(leveldb);

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
  leveldb = std::make_unique<TransactionalLevelDBDatabase>(
      std::move(ldb_state), nullptr, nullptr,
      kDefaultMaxOpenIteratorsPerDatabase);
  ASSERT_TRUE(leveldb);
  status = leveldb->Get(key, &got_value, &found);
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

TEST(LevelDBDatabaseTest, LastModified) {
  const std::string key("key");
  const std::string value("value");
  std::string put_value;
  SimpleComparator comparator;
  auto test_clock = std::make_unique<base::SimpleTestClock>();
  base::SimpleTestClock* clock_ptr = test_clock.get();
  clock_ptr->Advance(base::TimeDelta::FromHours(2));

  leveldb::Status status;
  scoped_refptr<LevelDBState> ldb_state;
  std::tie(ldb_state, status, std::ignore) = OpenLevelDB(base::FilePath());
  EXPECT_TRUE(status.ok());

  std::unique_ptr<TransactionalLevelDBDatabase> leveldb =
      std::make_unique<TransactionalLevelDBDatabase>(
          std::move(ldb_state), nullptr, nullptr,
          kDefaultMaxOpenIteratorsPerDatabase);
  ASSERT_TRUE(leveldb);
  leveldb->SetClockForTesting(std::move(test_clock));
  // Calling |Put| sets time modified.
  put_value = value;
  base::Time now_time = clock_ptr->Now();
  status = leveldb->Put(key, &put_value);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(now_time, leveldb->LastModified());

  // Calling |Remove| sets time modified.
  clock_ptr->Advance(base::TimeDelta::FromSeconds(200));
  now_time = clock_ptr->Now();
  status = leveldb->Remove(key);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(now_time, leveldb->LastModified());

  // Calling |Write| sets time modified
  clock_ptr->Advance(base::TimeDelta::FromMinutes(15));
  now_time = clock_ptr->Now();
  auto batch = LevelDBWriteBatch::Create();
  batch->Put(key, value);
  batch->Remove(key);
  status = leveldb->Write(*batch);
  EXPECT_TRUE(status.ok());
  EXPECT_EQ(now_time, leveldb->LastModified());
}

}  // namespace leveldb_unittest
}  // namespace content
