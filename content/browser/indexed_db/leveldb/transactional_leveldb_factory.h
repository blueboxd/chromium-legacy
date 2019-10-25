// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_TRANSACTIONAL_LEVELDB_FACTORY_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_TRANSACTIONAL_LEVELDB_FACTORY_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/scopes/leveldb_state.h"
#include "content/common/content_export.h"

namespace leveldb {
class Iterator;
}  // namespace leveldb

namespace content {
class TransactionalLevelDBDatabase;
class TransactionalLevelDBIterator;
class LevelDBScope;
class LevelDBScopes;
class LevelDBSnapshot;
class LevelDBDirectTransaction;
class TransactionalLevelDBTransaction;

class CONTENT_EXPORT TransactionalLevelDBFactory {
 public:
  virtual ~TransactionalLevelDBFactory() = default;

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
};

class CONTENT_EXPORT DefaultTransactionalLevelDBFactory
    : public TransactionalLevelDBFactory {
 public:
  DefaultTransactionalLevelDBFactory() = default;
  ~DefaultTransactionalLevelDBFactory() override = default;

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
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_TRANSACTIONAL_LEVELDB_FACTORY_H_
