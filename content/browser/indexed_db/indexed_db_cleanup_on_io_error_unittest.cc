// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cerrno>
#include <memory>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/leveldb/fake_leveldb_factory.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_database.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/leveldatabase/env_chromium.h"

using base::StringPiece;

namespace base {
class TaskRunner;
}

namespace content {
namespace {

TEST(IndexedDBIOErrorTest, CleanUpTest) {
  base::test::ScopedTaskEnvironment task_env;
  const url::Origin origin = url::Origin::Create(GURL("http://localhost:81"));
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath path = temp_directory.GetPath();

  auto task_runner = base::SequencedTaskRunnerHandle::Get();
  std::unique_ptr<IndexedDBBackingStore> backing_store = std::make_unique<
      IndexedDBBackingStore>(
      IndexedDBBackingStore::Mode::kInMemory, nullptr,
      indexed_db::LevelDBFactory::Get(), origin, path,
      std::make_unique<TransactionalLevelDBDatabase>(
          indexed_db::FakeLevelDBFactory::GetBrokenLevelDB(
              leveldb::Status::IOError("It's broken!"), path),
          indexed_db::LevelDBFactory::Get(), task_runner.get(),
          TransactionalLevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase),
      task_runner.get());
  leveldb::Status s = backing_store->Initialize(false);
  EXPECT_FALSE(s.ok());
}

TEST(IndexedDBNonRecoverableIOErrorTest, NuancedCleanupTest) {
  base::test::ScopedTaskEnvironment task_env;
  const url::Origin origin = url::Origin::Create(GURL("http://localhost:81"));
  base::ScopedTempDir temp_directory;
  ASSERT_TRUE(temp_directory.CreateUniqueTempDir());
  const base::FilePath path = temp_directory.GetPath();
  auto task_runner = base::SequencedTaskRunnerHandle::Get();
  leveldb::Status s;

  std::array<leveldb::Status, 4> errors = {
      MakeIOError("some filename", "some message", leveldb_env::kNewLogger,
                  base::File::FILE_ERROR_NO_SPACE),
      MakeIOError("some filename", "some message", leveldb_env::kNewLogger,
                  base::File::FILE_ERROR_NO_MEMORY),
      MakeIOError("some filename", "some message", leveldb_env::kNewLogger,
                  base::File::FILE_ERROR_IO),
      MakeIOError("some filename", "some message", leveldb_env::kNewLogger,
                  base::File::FILE_ERROR_FAILED)};
  for (leveldb::Status error_status : errors) {
    std::unique_ptr<IndexedDBBackingStore> backing_store = std::make_unique<
        IndexedDBBackingStore>(
        IndexedDBBackingStore::Mode::kInMemory, nullptr,
        indexed_db::LevelDBFactory::Get(), origin, path,
        std::make_unique<TransactionalLevelDBDatabase>(
            indexed_db::FakeLevelDBFactory::GetBrokenLevelDB(error_status,
                                                             path),
            indexed_db::LevelDBFactory::Get(), task_runner.get(),
            TransactionalLevelDBDatabase::kDefaultMaxOpenIteratorsPerDatabase),
        task_runner.get());
    leveldb::Status s = backing_store->Initialize(false);
    ASSERT_TRUE(s.IsIOError());
  }
}

}  // namespace
}  // namespace content
