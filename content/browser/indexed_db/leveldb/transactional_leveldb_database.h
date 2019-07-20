// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_LEVELDB_TRANSACTIONAL_LEVELDB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_LEVELDB_TRANSACTIONAL_LEVELDB_DATABASE_H_

#include <memory>
#include <string>

#include "base/containers/mru_cache.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/time/clock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "content/browser/indexed_db/scopes/leveldb_state.h"
#include "content/common/content_export.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace leveldb {
class Comparator;
class DB;
class Iterator;
class Env;
class Snapshot;
}  // namespace leveldb

namespace content {
class LevelDBComparator;
class TransactionalLevelDBDatabase;
class TransactionalLevelDBIterator;
class LevelDBWriteBatch;

namespace indexed_db {
class LevelDBFactory;
}  // namespace indexed_db

class LevelDBSnapshot {
 private:
  friend class TransactionalLevelDBDatabase;
  friend class TransactionalLevelDBTransaction;

  explicit LevelDBSnapshot(TransactionalLevelDBDatabase* db);
  ~LevelDBSnapshot();

  leveldb::DB* db_;
  const leveldb::Snapshot* snapshot_;

  DISALLOW_COPY_AND_ASSIGN(LevelDBSnapshot);
};

class CONTENT_EXPORT TransactionalLevelDBDatabase
    : public base::trace_event::MemoryDumpProvider {
 public:
  // Necessary because every iterator hangs onto leveldb blocks which can be
  // large. See https://crbug/696055.
  static const size_t kDefaultMaxOpenIteratorsPerDatabase = 50;

  // |max_open_cursors| cannot be 0.
  // All calls to this class should be done on |task_runner|.
  TransactionalLevelDBDatabase(
      scoped_refptr<LevelDBState> level_db_state,
      indexed_db::LevelDBFactory* factory,
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      size_t max_open_iterators);

  ~TransactionalLevelDBDatabase() override;

  leveldb::Status Put(const base::StringPiece& key, std::string* value);
  leveldb::Status Remove(const base::StringPiece& key);
  virtual leveldb::Status Get(const base::StringPiece& key,
                              std::string* value,
                              bool* found,
                              const LevelDBSnapshot* = 0);
  leveldb::Status Write(const LevelDBWriteBatch& write_batch);
  // Note: Use DefaultReadOptions() and then adjust any values afterwards.
  std::unique_ptr<TransactionalLevelDBIterator> CreateIterator(
      const leveldb::ReadOptions& options);
  const LevelDBComparator* Comparator() const {
    return level_db_state_->idb_comparator();
  }
  void Compact(const base::StringPiece& start, const base::StringPiece& stop);
  void CompactAll();

  leveldb::ReadOptions DefaultReadOptions();
  leveldb::ReadOptions DefaultReadOptions(const LevelDBSnapshot* snapshot);

  // base::trace_event::MemoryDumpProvider implementation.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  LevelDBState* leveldb_state() { return level_db_state_.get(); }
  leveldb::DB* db() { return level_db_state_->db(); }
  leveldb::Env* env() { return level_db_state_->in_memory_env(); }
  base::Time LastModified() const { return last_modified_; }

  indexed_db::LevelDBFactory* leveldb_class_factory() const {
    return class_factory_;
  }

  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

  base::WeakPtr<TransactionalLevelDBDatabase> AsWeakPtr() {
    return weak_factory_for_iterators_.GetWeakPtr();
  }

 private:
  friend class LevelDBSnapshot;
  friend class TransactionalLevelDBIteratorImpl;
  FRIEND_TEST_ALL_PREFIXES(IndexedDBTest, DeleteFailsIfDirectoryLocked);

  // Methods for iterator pooling.
  std::unique_ptr<leveldb::Iterator> CreateLevelDBIterator(
      const leveldb::Snapshot*);
  void OnIteratorUsed(TransactionalLevelDBIterator*);
  void OnIteratorDestroyed(TransactionalLevelDBIterator*);

  void CloseDatabase();

  scoped_refptr<LevelDBState> level_db_state_;
  indexed_db::LevelDBFactory* class_factory_;
  base::Time last_modified_;
  std::unique_ptr<base::Clock> clock_;

  struct DetachIteratorOnDestruct {
    DetachIteratorOnDestruct() {}
    explicit DetachIteratorOnDestruct(TransactionalLevelDBIterator* it)
        : it_(it) {}
    DetachIteratorOnDestruct(DetachIteratorOnDestruct&& that) {
      it_ = that.it_;
      that.it_ = nullptr;
    }
    ~DetachIteratorOnDestruct();

   private:
    TransactionalLevelDBIterator* it_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(DetachIteratorOnDestruct);
  };

  // Despite the type name, this object uses LRU eviction.
  base::HashingMRUCache<TransactionalLevelDBIterator*, DetachIteratorOnDestruct>
      iterator_lru_;

  // Recorded for UMA reporting.
  uint32_t num_iterators_ = 0;
  uint32_t max_iterators_ = 0;

  std::string file_name_for_tracing;

  base::WeakPtrFactory<TransactionalLevelDBDatabase>
      weak_factory_for_iterators_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_LEVELDB_TRANSACTIONAL_LEVELDB_DATABASE_H_
