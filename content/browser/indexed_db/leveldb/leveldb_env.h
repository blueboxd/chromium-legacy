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
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
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
}  // namespace leveldb

namespace content {
class TransactionalLevelDBDatabase;
class TransactionalLevelDBIterator;
class LevelDBScope;
class LevelDBScopes;
class LevelDBSnapshot;
class TransactionalLevelDBTransaction;
class LevelDBDirectTransaction;

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
    bool create_if_missing,
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
  // in-memory. The comparator names must match. If |create_if_missing| is
  // false and the database doesn't exist, then the returned tuple will be
  // {nullptr, leveldb::Status::NotFound("", ""), false}.
  virtual std::
      tuple<scoped_refptr<LevelDBState>, leveldb::Status, bool /* disk_full*/>
      OpenLevelDBState(const base::FilePath& file_name,
                       const leveldb::Comparator* ldb_comparator,
                       bool create_if_missing) = 0;

  // All calls to the LevelDBDatabase class should be done on |task_runner|.
  virtual std::unique_ptr<TransactionalLevelDBDatabase> CreateLevelDBDatabase(
      scoped_refptr<LevelDBState> state,
      std::unique_ptr<LevelDBScopes> scopes,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      size_t max_open_iterators) = 0;

  // A LevelDBDirectTransaction is a simple wrapper of a WriteBatch, which
  // writes it on |Commit|.
  virtual std::unique_ptr<LevelDBDirectTransaction>
  CreateLevelDBDirectTransaction(TransactionalLevelDBDatabase* db) = 0;

  // A LevelDBTransaction uses the LevelDBScope to write changes, and
  // appropriately flushes the scope for reads.
  virtual scoped_refptr<TransactionalLevelDBTransaction>
  CreateLevelDBTransaction(TransactionalLevelDBDatabase* db,
                           std::unique_ptr<LevelDBScope> scope) = 0;

  virtual std::unique_ptr<TransactionalLevelDBIterator> CreateIterator(
      std::unique_ptr<leveldb::Iterator> it,
      base::WeakPtr<TransactionalLevelDBDatabase> db,
      base::WeakPtr<TransactionalLevelDBTransaction> txn,
      std::unique_ptr<LevelDBSnapshot> snapshot) = 0;

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
                   const leveldb::Comparator* ldb_comparator,
                   bool create_if_missing) override;
  std::unique_ptr<TransactionalLevelDBDatabase> CreateLevelDBDatabase(
      scoped_refptr<LevelDBState> state,
      std::unique_ptr<LevelDBScopes> scopes,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      size_t max_open_iterators) override;
  std::unique_ptr<LevelDBDirectTransaction> CreateLevelDBDirectTransaction(
      TransactionalLevelDBDatabase* db) override;
  scoped_refptr<TransactionalLevelDBTransaction> CreateLevelDBTransaction(
      TransactionalLevelDBDatabase* db,
      std::unique_ptr<LevelDBScope> scope) override;
  std::unique_ptr<TransactionalLevelDBIterator> CreateIterator(
      std::unique_ptr<leveldb::Iterator> it,
      base::WeakPtr<TransactionalLevelDBDatabase> db,
      base::WeakPtr<TransactionalLevelDBTransaction> txn,
      std::unique_ptr<LevelDBSnapshot> snapshot) override;
  leveldb::Status DestroyLevelDB(
      scoped_refptr<LevelDBState> output_state) override;
  leveldb::Status DestroyLevelDB(const base::FilePath& path) override;
};

}  // namespace indexed_db
}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_LEVELDB_ENV_H_
