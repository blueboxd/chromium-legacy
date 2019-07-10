// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ENV_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ENV_H_

#include <memory>
#include <tuple>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "content/browser/indexed_db/leveldb/leveldb_comparator.h"
#include "content/browser/indexed_db/scopes/leveldb_state.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace leveldb {
class Iterator;
class Snapshot;
}  // namespace leveldb

namespace content {
class TransactionalLevelDBIteratorImpl;
class TransactionalLevelDBDatabase;
class TransactionalLevelDBTransaction;

// The leveldb::Env used by the Indexed DB backend.
class LevelDBEnv : public leveldb_env::ChromiumEnv {
 public:

  CONTENT_EXPORT static LevelDBEnv* Get();

 private:
  friend class base::NoDestructor<LevelDBEnv>;
  LevelDBEnv();
};

namespace indexed_db {

// Visible for testing.
CONTENT_EXPORT leveldb_env::Options GetLevelDBOptions(
    leveldb::Env* env,
    const leveldb::Comparator* comparator,
    size_t write_buffer_size,
    bool paranoid_checks);

// Factory class used to open leveldb databases, and stores all necessary
// objects in a LevelDBState. This interface exists so that it can be mocked out
// for tests.
class CONTENT_EXPORT LevelDBFactory {
 public:
  using GetterCallback = LevelDBFactory*();

  // Returns a singleton factory, which can be customized below for testing.
  // Note: it is best to avoid using this method, and instead acquiring a
  // LevelDBFactory through dependency injection.
  static LevelDBFactory* Get();

  static void SetFactoryGetterForTesting(LevelDBFactory::GetterCallback* cb);

  virtual ~LevelDBFactory() = default;

  virtual std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status> OpenDB(
      const leveldb_env::Options& options,
      const std::string& name) = 0;

  // Opens a leveldb database with the given comparators and populates it in
  // |output_state|. If the |file_name| is empty, then the database will be
  // in-memory. The comparator names must match.
  virtual std::
      tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /* disk_full*/>
      OpenLevelDBState(const base::FilePath& file_name,
                       const LevelDBComparator* idb_comparator,
                       const leveldb::Comparator* ldb_comparator) = 0;

  virtual std::unique_ptr<TransactionalLevelDBIteratorImpl> CreateIteratorImpl(
      std::unique_ptr<leveldb::Iterator> iterator,
      TransactionalLevelDBDatabase* db,
      const leveldb::Snapshot* snapshot) = 0;

  virtual scoped_refptr<TransactionalLevelDBTransaction>
  CreateLevelDBTransaction(TransactionalLevelDBDatabase* db) = 0;

  // A somewhat safe way to destroy a leveldb database. This asserts that there
  // are no other references to the given LevelDBState, and deletes the database
  // on disk. |level_db_state| must only have one ref.
  virtual leveldb::Status DestroyLevelDB(
      scoped_refptr<LevelDBState> output_state) = 0;

  // Assumes that there is no leveldb database currently running for this path.
  virtual leveldb::Status DestroyLevelDB(const base::FilePath& path) = 0;
};

class CONTENT_EXPORT DefaultLevelDBFactory : public LevelDBFactory {
 public:
  DefaultLevelDBFactory() = default;
  ~DefaultLevelDBFactory() override {}

  std::tuple<std::unique_ptr<leveldb::DB>, leveldb::Status> OpenDB(
      const leveldb_env::Options& options,
      const std::string& name) override;

  std::tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /* disk_full*/>
  OpenLevelDBState(const base::FilePath& file_name,
                   const LevelDBComparator* idb_comparator,
                   const leveldb::Comparator* ldb_comparator) override;
  std::unique_ptr<TransactionalLevelDBIteratorImpl> CreateIteratorImpl(
      std::unique_ptr<leveldb::Iterator> iterator,
      TransactionalLevelDBDatabase* db,
      const leveldb::Snapshot* snapshot) override;
  scoped_refptr<TransactionalLevelDBTransaction> CreateLevelDBTransaction(
      TransactionalLevelDBDatabase* db) override;
  leveldb::Status DestroyLevelDB(
      scoped_refptr<LevelDBState> output_state) override;
  leveldb::Status DestroyLevelDB(const base::FilePath& path) override;
};

}  // namespace indexed_db
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ENV_H_
