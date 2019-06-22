// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/scopes/leveldb_scopes_test_utils.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/bind_test_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace content {

constexpr const size_t LevelDBScopesTestBase::kWriteBatchSizeForTesting;

LevelDBScopesTestBase::LevelDBScopesTestBase() = default;
LevelDBScopesTestBase::~LevelDBScopesTestBase() = default;

void LevelDBScopesTestBase::SetUp() {
  large_string_.assign(kWriteBatchSizeForTesting + 1, 'e');
}

void LevelDBScopesTestBase::TearDown() {
  if (leveldb_) {
    base::RunLoop loop;
    if (leveldb_->RequestDestruction(loop.QuitClosure(),
                                     base::SequencedTaskRunnerHandle::Get())) {
      leveldb_.reset();
      loop.Run();
    }
    leveldb_.reset();
    if (temp_directory_.IsValid()) {
      indexed_db::LevelDBFactory::Get()->DestroyLevelDB(
          temp_directory_.GetPath());
      ASSERT_TRUE(temp_directory_.Delete());
    }
  }
}

void LevelDBScopesTestBase::SetUpRealDatabase() {
  if (leveldb_)
    TearDown();
  ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
  leveldb::Status status;
  std::tie(leveldb_, status, std::ignore) =
      indexed_db::LevelDBFactory::Get()->OpenLevelDBState(
          temp_directory_.GetPath(), LevelDBComparator::BytewiseComparator(),
          leveldb::BytewiseComparator());
  ASSERT_TRUE(status.ok());
  ASSERT_TRUE(leveldb_);
}

void LevelDBScopesTestBase::SetUpBreakableDB(
    base::OnceCallback<void(leveldb::Status)>* callback) {
  if (leveldb_)
    TearDown();
  ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());

  leveldb_env::Options options = indexed_db::GetLevelDBOptions(
      LevelDBEnv::Get(), leveldb::BytewiseComparator(),
      leveldb_env::WriteBufferSize(
          base::SysInfo::AmountOfTotalDiskSpace(temp_directory_.GetPath())),
      /*paranoid_checks=*/true);

  leveldb::Status status;
  indexed_db::FakeLevelDBFactory factory;
  std::unique_ptr<leveldb::DB> temp_real_db;
  std::tie(temp_real_db, status) =
      factory.OpenDB(options, temp_directory_.GetPath().AsUTF8Unsafe());
  ASSERT_TRUE(status.ok());
  ASSERT_TRUE(temp_real_db);

  std::unique_ptr<leveldb::DB> db;
  std::tie(db, *callback) = indexed_db::FakeLevelDBFactory::CreateBreakableDB(
      std::move(temp_real_db));
  ASSERT_TRUE(db);
  leveldb_ = LevelDBState::CreateForDiskDB(
      leveldb::BytewiseComparator(), LevelDBComparator::BytewiseComparator(),
      std::move(db), temp_directory_.GetPath());
}

void LevelDBScopesTestBase::SetUpFlakyDB(
    std::queue<indexed_db::FakeLevelDBFactory::FlakePoint> flake_points) {
  if (leveldb_)
    TearDown();
  ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
  leveldb::Status status;

  indexed_db::FakeLevelDBFactory fake_factory;

  leveldb_env::Options options = indexed_db::GetLevelDBOptions(
      LevelDBEnv::Get(), leveldb::BytewiseComparator(),
      leveldb_env::WriteBufferSize(
          base::SysInfo::AmountOfTotalDiskSpace(temp_directory_.GetPath())),
      /*paranoid_checks=*/true);
  std::unique_ptr<leveldb::DB> temp_db;
  std::tie(temp_db, status) =
      fake_factory.OpenDB(options, temp_directory_.GetPath().AsUTF8Unsafe());
  ASSERT_TRUE(status.ok());
  ASSERT_TRUE(temp_db);

  std::unique_ptr<leveldb::DB> flaky_db =
      indexed_db::FakeLevelDBFactory::CreateFlakyDB(std::move(temp_db),
                                                    std::move(flake_points));

  fake_factory.EnqueueNextOpenDBResult(std::move(flaky_db),
                                       leveldb::Status::OK());
  std::tie(leveldb_, status, std::ignore) = fake_factory.OpenLevelDBState(
      temp_directory_.GetPath(), LevelDBComparator::BytewiseComparator(),
      leveldb::BytewiseComparator());
  ASSERT_TRUE(status.ok());
  ASSERT_TRUE(leveldb_);
}

void LevelDBScopesTestBase::WriteScopesMetadata(int64_t scope_number,
                                                bool ignore_cleanup_tasks) {
  value_buffer_.clear();
  auto key = scopes_encoder_.ScopeMetadataKey(metadata_prefix_, scope_number);
  metadata_buffer_.set_ignore_cleanup_tasks(ignore_cleanup_tasks);
  metadata_buffer_.SerializeToString(&value_buffer_);
  leveldb::Status s =
      leveldb_->db()->Put(leveldb::WriteOptions(), key, value_buffer_);
  ASSERT_TRUE(s.ok());
  metadata_buffer_.Clear();
}

void LevelDBScopesTestBase::WriteUndoTask(int64_t scope_number,
                                          int64_t sequence_number) {
  value_buffer_.clear();
  auto key = scopes_encoder_.UndoTaskKey(metadata_prefix_, scope_number,
                                         sequence_number);
  undo_task_buffer_.SerializeToString(&value_buffer_);
  leveldb::Status s =
      leveldb_->db()->Put(leveldb::WriteOptions(), key, value_buffer_);
  ASSERT_TRUE(s.ok());
  undo_task_buffer_.Clear();
}

void LevelDBScopesTestBase::WriteCleanupTask(int64_t scope_number,
                                             int64_t sequence_number) {
  value_buffer_.clear();
  auto key = scopes_encoder_.CleanupTaskKey(metadata_prefix_, scope_number,
                                            sequence_number);
  cleanup_task_buffer_.SerializeToString(&value_buffer_);
  leveldb::Status s =
      leveldb_->db()->Put(leveldb::WriteOptions(), key, value_buffer_);
  ASSERT_TRUE(s.ok());
  cleanup_task_buffer_.Clear();
}

void LevelDBScopesTestBase::WriteLargeValue(const std::string& key) {
  leveldb::Status s =
      leveldb_->db()->Put(leveldb::WriteOptions(), key, large_string_);
  ASSERT_TRUE(s.ok());
}

leveldb::Status LevelDBScopesTestBase::LoadAt(const std::string& key) {
  return leveldb_->db()->Get(leveldb::ReadOptions(), key, &value_buffer_);
}

leveldb::Status LevelDBScopesTestBase::LoadScopeMetadata(int64_t scope_number) {
  auto key = scopes_encoder_.ScopeMetadataKey(metadata_prefix_, scope_number);
  return leveldb_->db()->Get(leveldb::ReadOptions(), key, &value_buffer_);
}

leveldb::Status LevelDBScopesTestBase::LoadUndoTask(int64_t scope_number,
                                                    int64_t sequence_number) {
  auto key = scopes_encoder_.UndoTaskKey(metadata_prefix_, scope_number,
                                         sequence_number);
  return leveldb_->db()->Get(leveldb::ReadOptions(), key, &value_buffer_);
}

leveldb::Status LevelDBScopesTestBase::LoadCleanupTask(
    int64_t scope_number,
    int64_t sequence_number) {
  auto key = scopes_encoder_.CleanupTaskKey(metadata_prefix_, scope_number,
                                            sequence_number);
  return leveldb_->db()->Get(leveldb::ReadOptions(), key, &value_buffer_);
}

bool LevelDBScopesTestBase::IsPrefixedRangeEmptyInDB(leveldb::Slice key) {
  std::unique_ptr<leveldb::Iterator> it =
      base::WrapUnique(leveldb_->db()->NewIterator(leveldb::ReadOptions()));
  it->Seek(key);
  if (it->Valid() && it->key().starts_with(key))
    return false;
  return true;
}

bool LevelDBScopesTestBase::IsScopeCleanedUp(int64_t scope_number) {
  return IsPrefixedRangeEmptyInDB(
             scopes_encoder_.TasksKeyPrefix(metadata_prefix_, scope_number)) &&
         IsPrefixedRangeEmptyInDB(
             scopes_encoder_.TasksKeyPrefix(metadata_prefix_, scope_number));
}

bool LevelDBScopesTestBase::ScopeDataExistsOnDisk() {
  return !IsPrefixedRangeEmptyInDB(
             scopes_encoder_.ScopeMetadataPrefix(metadata_prefix_)) ||
         !IsPrefixedRangeEmptyInDB(
             scopes_encoder_.TasksKeyPrefix(metadata_prefix_));
}

ScopesLockManager::ScopeLockRequest
LevelDBScopesTestBase::CreateSimpleSharedLock() {
  return {0,
          {simple_lock_begin_, simple_lock_end_},
          ScopesLockManager::LockType::kShared};
}

ScopesLockManager::ScopeLockRequest
LevelDBScopesTestBase::CreateSimpleExclusiveLock() {
  return {0,
          {simple_lock_begin_, simple_lock_end_},
          ScopesLockManager::LockType::kExclusive};
}

ScopesLockManager::ScopeLockRequest LevelDBScopesTestBase::CreateSharedLock(
    int i) {
  return {0,
          {base::StringPrintf("%010d", i * 2),
           base::StringPrintf("%010d", i * 2 + 1)},
          ScopesLockManager::LockType::kShared};
}

ScopesLockManager::ScopeLockRequest LevelDBScopesTestBase::CreateExclusiveLock(
    int i) {
  return {0,
          {base::StringPrintf("%010d", i * 2),
           base::StringPrintf("%010d", i * 2 + 1)},
          ScopesLockManager::LockType::kExclusive};
}

}  // namespace content
