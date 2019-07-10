// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/leveldb_env.h"

#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "content/browser/indexed_db/indexed_db_leveldb_operations.h"
#include "content/browser/indexed_db/indexed_db_reporting.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_iterator_impl.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_transaction.h"
#include "third_party/leveldatabase/leveldb_chrome.h"
#include "third_party/leveldatabase/src/include/leveldb/filter_policy.h"

namespace content {

LevelDBEnv::LevelDBEnv() : ChromiumEnv("LevelDBEnv.IDB") {}

LevelDBEnv* LevelDBEnv::Get() {
  static base::NoDestructor<LevelDBEnv> g_leveldb_env;
  return g_leveldb_env.get();
}

namespace indexed_db {

leveldb_env::Options GetLevelDBOptions(leveldb::Env* env,
                                       const leveldb::Comparator* comparator,
                                       size_t write_buffer_size,
                                       bool paranoid_checks) {
  static const leveldb::FilterPolicy* kIDBFilterPolicy =
      leveldb::NewBloomFilterPolicy(10);
  leveldb_env::Options options;
  options.comparator = comparator;
  options.create_if_missing = true;
  options.paranoid_checks = paranoid_checks;
  options.filter_policy = kIDBFilterPolicy;
  options.compression = leveldb::kSnappyCompression;
  options.write_buffer_size = write_buffer_size;
  // For info about the troubles we've run into with this parameter, see:
  // https://crbug.com/227313#c11
  options.max_open_files = 80;
  options.env = env;
  options.block_cache = leveldb_chrome::GetSharedWebBlockCache();
  return options;
}

static LevelDBFactory::GetterCallback* s_ldb_factory_getter;

// static
LevelDBFactory* LevelDBFactory::Get() {
  static base::NoDestructor<DefaultLevelDBFactory> singleton;
  if (s_ldb_factory_getter)
    return (*s_ldb_factory_getter)();
  return singleton.get();
}

// static
void LevelDBFactory::SetFactoryGetterForTesting(
    LevelDBFactory::GetterCallback* cb) {
  s_ldb_factory_getter = cb;
}

std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status>
DefaultLevelDBFactory::OpenDB(const leveldb_env::Options& options,
                              const std::string& name) {
  std::unique_ptr<leveldb::DB> db;
  leveldb::Status s = leveldb_env::OpenDB(options, name, &db);
  return {std::move(db), s};
}

std::tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /* disk_full*/>
DefaultLevelDBFactory::OpenLevelDBState(
    const base::FilePath& file_name,
    const LevelDBComparator* idb_comparator,
    const leveldb::Comparator* ldb_comparator) {
  // Please see docs/open_and_verify_leveldb_database.code2flow, and the
  // generated pdf (from https://code2flow.com).
  // The intended strategy here is to have this function match that flowchart,
  // where the flowchart should be seen as the 'master' logic template. Please
  // check the git history of both to make sure they are supposed to be in sync.
  IDB_TRACE("indexed_db::OpenLDB");
  DCHECK(strcmp(ldb_comparator->Name(), idb_comparator->Name()) == 0);
  base::TimeTicks begin_time = base::TimeTicks::Now();
  leveldb::Status status;
  std::unique_ptr<leveldb::DB> db;

  if (file_name.empty()) {
    std::unique_ptr<leveldb::Env> in_memory_env =
        leveldb_chrome::NewMemEnv("indexed-db", LevelDBEnv::Get());

    constexpr int64_t kBytesInOneMegabyte = 1024 * 1024;
    leveldb_env::Options in_memory_options =
        GetLevelDBOptions(in_memory_env.get(), ldb_comparator,
                          /* default of 4MB */ 4 * kBytesInOneMegabyte,
                          /*paranoid_checks=*/false);
    std::tie(db, status) = OpenDB(in_memory_options, std::string());
    if (!status.ok()) {
      LOG(ERROR) << "Failed to open in-memory LevelDB database: "
                 << status.ToString();
      // Must match the other returns in this function for RVO.
      return {nullptr, status, false};
    }

    return {LevelDBState::CreateForInMemoryDB(
                std::move(in_memory_env), ldb_comparator, idb_comparator,
                std::move(db), "in-memory-database"),
            status, false};
  }

  leveldb_env::Options options =
      GetLevelDBOptions(LevelDBEnv::Get(), ldb_comparator,
                        leveldb_env::WriteBufferSize(
                            base::SysInfo::AmountOfTotalDiskSpace(file_name)),
                        /*paranoid_checks=*/true);

  // ChromiumEnv assumes UTF8, converts back to FilePath before using.
  std::tie(db, status) = OpenDB(options, file_name.AsUTF8Unsafe());
  if (!status.ok()) {
    ReportLevelDBError("WebCore.IndexedDB.LevelDBOpenErrors", status);

    constexpr int64_t kBytesInOneKilobyte = 1024;
    int64_t free_disk_space_bytes =
        base::SysInfo::AmountOfFreeDiskSpace(file_name);
    bool below_100kb = free_disk_space_bytes != -1 &&
                       free_disk_space_bytes < 100 * kBytesInOneKilobyte;

    // Disks with <100k of free space almost never succeed in opening a
    // leveldb database.
    bool is_disk_full = below_100kb || leveldb_env::IndicatesDiskFull(status);

    LOG(ERROR) << "Failed to open LevelDB database from "
               << file_name.AsUTF8Unsafe() << "," << status.ToString();
    // Must match the other returns in this function for RVO.
    return {nullptr, status, is_disk_full};
  }

  UMA_HISTOGRAM_MEDIUM_TIMES("WebCore.IndexedDB.LevelDB.OpenTime",
                             base::TimeTicks::Now() - begin_time);

  // Must match the other returns in this function for RVO.
  return {LevelDBState::CreateForDiskDB(ldb_comparator, idb_comparator,
                                        std::move(db), std::move(file_name)),
          status, false};
}

scoped_refptr<TransactionalLevelDBTransaction>
DefaultLevelDBFactory::CreateLevelDBTransaction(
    TransactionalLevelDBDatabase* db) {
  return base::WrapRefCounted(new TransactionalLevelDBTransaction(db));
}

std::unique_ptr<TransactionalLevelDBIteratorImpl>
DefaultLevelDBFactory::CreateIteratorImpl(
    std::unique_ptr<leveldb::Iterator> iterator,
    TransactionalLevelDBDatabase* db,
    const leveldb::Snapshot* snapshot) {
  return base::WrapUnique(
      new TransactionalLevelDBIteratorImpl(std::move(iterator), db, snapshot));
}

leveldb::Status DefaultLevelDBFactory::DestroyLevelDB(
    scoped_refptr<LevelDBState> level_db_state) {
  DCHECK(level_db_state);
  DCHECK(!level_db_state->in_memory_env() &&
         !level_db_state->database_path().empty())
      << "Cannot destroy an in-memory databases.";
  DCHECK(level_db_state->HasOneRef())
      << "Cannot destroy the database when someone else is keeping the state "
         "alive.";
  leveldb_env::Options options;
  options.env = LevelDBEnv::Get();
  std::string file_path = level_db_state->database_path().AsUTF8Unsafe();
  level_db_state.reset();
  // ChromiumEnv assumes UTF8, converts back to FilePath before using.
  return leveldb::DestroyDB(std::move(file_path), options);
}

leveldb::Status DefaultLevelDBFactory::DestroyLevelDB(
    const base::FilePath& path) {
  leveldb_env::Options options;
  options.env = LevelDBEnv::Get();
  return leveldb::DestroyDB(path.AsUTF8Unsafe(), options);
}

}  // namespace indexed_db
}  // namespace content
