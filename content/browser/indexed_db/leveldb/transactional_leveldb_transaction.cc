// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/transactional_leveldb_transaction.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/leveldb/leveldb_write_batch.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_database.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

using base::StringPiece;

namespace {

// Tests if the given key is before the end of a range, which
// may have an open (exclusive) or closed (inclusive) bound.
bool IsKeyBeforeEndOfRange(const content::LevelDBComparator* comparator,
                           const StringPiece& key,
                           const StringPiece& end,
                           bool open) {
  return (open ? comparator->Compare(key, end) < 0
               : comparator->Compare(key, end) <= 0);
}
}  // namespace

namespace content {

TransactionalLevelDBTransaction::TransactionalLevelDBTransaction(
    TransactionalLevelDBDatabase* db)
    : db_(db),
      snapshot_(db),
      comparator_(db->Comparator()),
      data_comparator_(comparator_),
      data_(data_comparator_) {}

TransactionalLevelDBTransaction::Record::Record() {}
TransactionalLevelDBTransaction::Record::~Record() {}

TransactionalLevelDBTransaction::~TransactionalLevelDBTransaction() {}

void TransactionalLevelDBTransaction::Set(const StringPiece& key,
                                          std::string* value,
                                          bool deleted) {
  DCHECK(!finished_);
  auto it = data_.find(key);

  if (it == data_.end()) {
    std::unique_ptr<Record> record = std::make_unique<Record>();
    size_ += SizeOfRecordInMap(key.size()) + value->size();
    record->key.assign(key.begin(), key.end() - key.begin());
    record->value.swap(*value);
    record->deleted = deleted;
    data_[record->key] = std::move(record);
    NotifyIterators();
    return;
  }
  size_ += value->size();
  it->second->value.swap(*value);
  size_ -= value->size();
  it->second->deleted = deleted;
}

void TransactionalLevelDBTransaction::Put(const StringPiece& key,
                                          std::string* value) {
  Set(key, value, false);
}

void TransactionalLevelDBTransaction::Remove(const StringPiece& key) {
  std::string empty;
  Set(key, &empty, true);
}

leveldb::Status TransactionalLevelDBTransaction::RemoveRange(
    const StringPiece& begin,
    const StringPiece& end,
    bool upper_open) {
  leveldb::Status s;
  bool dirty = false;
  {
    // Scope this iterator so it is deleted before other iterators are
    // notified.
    std::unique_ptr<TransactionIterator> it = TransactionIterator::Create(this);
    for (s = it->Seek(begin);
         s.ok() && it->IsValid() &&
         IsKeyBeforeEndOfRange(comparator_, it->Key(), end, upper_open);
         s = it->Next()) {
      it->Delete();
      dirty = true;
    }
  }
  if (dirty)
    NotifyIterators();
  return s;
}

leveldb::Status TransactionalLevelDBTransaction::Get(const StringPiece& key,
                                                     std::string* value,
                                                     bool* found) {
  *found = false;
  DCHECK(!finished_);
  std::string string_key(key.begin(), key.end() - key.begin());
  DataType::const_iterator it = data_.find(string_key);

  if (it != data_.end()) {
    if (it->second->deleted)
      return leveldb::Status::OK();

    *value = it->second->value;
    *found = true;
    return leveldb::Status::OK();
  }

  leveldb::Status s = db_->Get(key, value, found, &snapshot_);
  if (!s.ok())
    DCHECK(!*found);
  return s;
}

leveldb::Status TransactionalLevelDBTransaction::Commit() {
  DCHECK(!finished_);
  IDB_TRACE("LevelDBTransaction::Commit");

  if (data_.empty()) {
    finished_ = true;
    return leveldb::Status::OK();
  }

  std::unique_ptr<LevelDBWriteBatch> write_batch = LevelDBWriteBatch::Create();

  auto it = data_.begin();
  while (it != data_.end()) {
    if (!it->second->deleted)
      write_batch->Put(it->first, it->second->value);
    else
      write_batch->Remove(it->first);
    data_.erase(it++);
  }

  DCHECK(data_.empty());

  leveldb::Status s = db_->Write(*write_batch);
  if (s.ok())
    finished_ = true;
  return s;
}

void TransactionalLevelDBTransaction::Rollback() {
  DCHECK(!finished_);
  finished_ = true;
  data_.clear();
}

std::unique_ptr<TransactionalLevelDBIterator>
TransactionalLevelDBTransaction::CreateIterator() {
  return TransactionIterator::Create(this);
}

std::unique_ptr<TransactionalLevelDBTransaction::DataIterator>
TransactionalLevelDBTransaction::DataIterator::Create(
    TransactionalLevelDBTransaction* transaction) {
  return base::WrapUnique(new DataIterator(transaction));
}

constexpr uint64_t TransactionalLevelDBTransaction::SizeOfRecordInMap(
    size_t key_size) {
  return sizeof(Record) + key_size * 2;
}

bool TransactionalLevelDBTransaction::DataIterator::IsValid() const {
  return iterator_ != data_->end();
}

leveldb::Status TransactionalLevelDBTransaction::DataIterator::SeekToLast() {
  iterator_ = data_->end();
  if (iterator_ != data_->begin())
    --iterator_;
  return leveldb::Status::OK();
}

leveldb::Status TransactionalLevelDBTransaction::DataIterator::Seek(
    const StringPiece& target) {
  iterator_ = data_->lower_bound(target);
  return leveldb::Status::OK();
}

leveldb::Status TransactionalLevelDBTransaction::DataIterator::Next() {
  DCHECK(IsValid());
  ++iterator_;
  return leveldb::Status::OK();
}

leveldb::Status TransactionalLevelDBTransaction::DataIterator::Prev() {
  DCHECK(IsValid());
  if (iterator_ != data_->begin())
    --iterator_;
  else
    iterator_ = data_->end();
  return leveldb::Status::OK();
}

StringPiece TransactionalLevelDBTransaction::DataIterator::Key() const {
  DCHECK(IsValid());
  return iterator_->first;
}

StringPiece TransactionalLevelDBTransaction::DataIterator::Value() const {
  DCHECK(IsValid());
  DCHECK(!IsDeleted());
  return iterator_->second->value;
}

bool TransactionalLevelDBTransaction::DataIterator::IsDeleted() const {
  DCHECK(IsValid());
  return iterator_->second->deleted;
}

void TransactionalLevelDBTransaction::DataIterator::Delete() {
  DCHECK(IsValid());
  iterator_->second->deleted = true;
  iterator_->second->value.clear();
}

TransactionalLevelDBTransaction::DataIterator::~DataIterator() {}

TransactionalLevelDBTransaction::DataIterator::DataIterator(
    TransactionalLevelDBTransaction* transaction)
    : data_(&transaction->data_), iterator_(data_->end()) {}

std::unique_ptr<TransactionalLevelDBTransaction::TransactionIterator>
TransactionalLevelDBTransaction::TransactionIterator::Create(
    scoped_refptr<TransactionalLevelDBTransaction> transaction) {
  return base::WrapUnique(new TransactionIterator(transaction));
}

TransactionalLevelDBTransaction::TransactionIterator::TransactionIterator(
    scoped_refptr<TransactionalLevelDBTransaction> transaction)
    : transaction_(transaction),
      comparator_(transaction_->comparator_),
      data_iterator_(DataIterator::Create(transaction_.get())),
      db_iterator_(transaction_->db_->CreateIterator(
          transaction_->db_->DefaultReadOptions(&transaction_->snapshot_))) {
  transaction_->RegisterIterator(this);
}

TransactionalLevelDBTransaction::TransactionIterator::~TransactionIterator() {
  transaction_->UnregisterIterator(this);
}

bool TransactionalLevelDBTransaction::TransactionIterator::IsValid() const {
  return !!current_;
}

leveldb::Status
TransactionalLevelDBTransaction::TransactionIterator::SeekToLast() {
  leveldb::Status s = data_iterator_->SeekToLast();
  DCHECK(s.ok());
  s = db_iterator_->SeekToLast();
  if (!s.ok())
    return s;
  direction_ = REVERSE;

  HandleConflictsAndDeletes();
  SetCurrentIteratorToLargestKey();
  return s;
}

leveldb::Status TransactionalLevelDBTransaction::TransactionIterator::Seek(
    const StringPiece& target) {
  leveldb::Status s = data_iterator_->Seek(target);
  DCHECK(s.ok());
  s = db_iterator_->Seek(target);
  if (!s.ok())
    return s;
  direction_ = FORWARD;

  HandleConflictsAndDeletes();
  SetCurrentIteratorToSmallestKey();
  return s;
}

leveldb::Status TransactionalLevelDBTransaction::TransactionIterator::Next() {
  DCHECK(IsValid());
  if (data_changed_)
    RefreshDataIterator();

  leveldb::Status s;
  if (direction_ != FORWARD) {
    // Ensure the non-current iterator is positioned after Key().

    TransactionalLevelDBIterator* non_current = (current_ == db_iterator_.get())
                                                    ? data_iterator_.get()
                                                    : db_iterator_.get();

    non_current->Seek(Key());
    if (non_current->IsValid() &&
        !comparator_->Compare(non_current->Key(), Key())) {
      // Take an extra step so the non-current key is
      // strictly greater than Key().
      s = non_current->Next();
      if (!s.ok())
        return s;
    }
    DCHECK(!non_current->IsValid() ||
           comparator_->Compare(non_current->Key(), Key()) > 0);

    direction_ = FORWARD;
  }

  s = current_->Next();
  if (!s.ok())
    return s;
  HandleConflictsAndDeletes();
  SetCurrentIteratorToSmallestKey();
  return leveldb::Status::OK();
}

leveldb::Status TransactionalLevelDBTransaction::TransactionIterator::Prev() {
  DCHECK(IsValid());
  leveldb::Status s;
  if (data_changed_)
    RefreshDataIterator();

  if (direction_ != REVERSE) {
    // Ensure the non-current iterator is positioned before Key().

    TransactionalLevelDBIterator* non_current = (current_ == db_iterator_.get())
                                                    ? data_iterator_.get()
                                                    : db_iterator_.get();

    s = non_current->Seek(Key());
    if (!s.ok())
      return s;
    if (non_current->IsValid()) {
      // Iterator is at first entry >= Key().
      // Step back once to entry < key.
      // This is why we don't check for the keys being the same before
      // stepping, like we do in Next() above.
      non_current->Prev();
    } else {
      // Iterator has no entries >= Key(). Position at last entry.
      non_current->SeekToLast();
    }
    DCHECK(!non_current->IsValid() ||
           comparator_->Compare(non_current->Key(), Key()) < 0);

    direction_ = REVERSE;
  }

  s = current_->Prev();
  if (!s.ok())
    return s;
  HandleConflictsAndDeletes();
  SetCurrentIteratorToLargestKey();
  return leveldb::Status::OK();
}

StringPiece TransactionalLevelDBTransaction::TransactionIterator::Key() const {
  DCHECK(IsValid());
  if (data_changed_)
    RefreshDataIterator();
  return current_->Key();
}

StringPiece TransactionalLevelDBTransaction::TransactionIterator::Value()
    const {
  DCHECK(IsValid());
  if (data_changed_)
    RefreshDataIterator();
  return current_->Value();
}
bool TransactionalLevelDBTransaction::TransactionIterator::IsDetached() const {
  return db_iterator_->IsDetached();
}

void TransactionalLevelDBTransaction::TransactionIterator::DataChanged() {
  data_changed_ = true;
}

void TransactionalLevelDBTransaction::TransactionIterator::Delete() {
  DCHECK(IsValid());
  if (current_ == data_iterator_.get()) {
    transaction_->size_ -= data_iterator_->Value().size();
    data_iterator_->Delete();
  } else {
    std::unique_ptr<Record> record = std::make_unique<Record>();
    record->key = Key().as_string();
    record->deleted = true;
    transaction_->size_ +=
        TransactionalLevelDBTransaction::SizeOfRecordInMap(record->key.size());
    transaction_->data_[record->key] = std::move(record);
  }
}

void TransactionalLevelDBTransaction::TransactionIterator::RefreshDataIterator()
    const {
  DCHECK(data_changed_);

  data_changed_ = false;

  if (data_iterator_->IsValid() && data_iterator_.get() == current_) {
    return;
  }

  if (db_iterator_->IsValid()) {
    // There could be new records in data that we should iterate over.

    if (direction_ == FORWARD) {
      // Try to seek data iterator to something greater than the db iterator.
      data_iterator_->Seek(db_iterator_->Key());
      if (data_iterator_->IsValid() &&
          !comparator_->Compare(data_iterator_->Key(), db_iterator_->Key())) {
        // If equal, take another step so the data iterator is strictly greater.
        data_iterator_->Next();
      }
    } else {
      // If going backward, seek to a key less than the db iterator.
      DCHECK_EQ(REVERSE, direction_);
      data_iterator_->Seek(db_iterator_->Key());
      if (data_iterator_->IsValid())
        data_iterator_->Prev();
    }
  }
}

bool TransactionalLevelDBTransaction::TransactionIterator::DataIteratorIsLower()
    const {
  return comparator_->Compare(data_iterator_->Key(), db_iterator_->Key()) < 0;
}

bool TransactionalLevelDBTransaction::TransactionIterator::
    DataIteratorIsHigher() const {
  return comparator_->Compare(data_iterator_->Key(), db_iterator_->Key()) > 0;
}

void TransactionalLevelDBTransaction::TransactionIterator::
    HandleConflictsAndDeletes() {
  bool loop = true;

  while (loop) {
    loop = false;

    if (data_iterator_->IsValid() && db_iterator_->IsValid() &&
        !comparator_->Compare(data_iterator_->Key(), db_iterator_->Key())) {
      // For equal keys, the data iterator takes precedence, so move the
      // database iterator another step.
      if (direction_ == FORWARD)
        db_iterator_->Next();
      else
        db_iterator_->Prev();
    }

    // Skip over delete markers in the data iterator until it catches up with
    // the db iterator.
    if (data_iterator_->IsValid() && data_iterator_->IsDeleted()) {
      if (direction_ == FORWARD &&
          (!db_iterator_->IsValid() || DataIteratorIsLower())) {
        data_iterator_->Next();
        loop = true;
      } else if (direction_ == REVERSE &&
                 (!db_iterator_->IsValid() || DataIteratorIsHigher())) {
        data_iterator_->Prev();
        loop = true;
      }
    }
  }
}

void TransactionalLevelDBTransaction::TransactionIterator::
    SetCurrentIteratorToSmallestKey() {
  TransactionalLevelDBIterator* smallest = nullptr;

  if (data_iterator_->IsValid())
    smallest = data_iterator_.get();

  if (db_iterator_->IsValid()) {
    if (!smallest ||
        comparator_->Compare(db_iterator_->Key(), smallest->Key()) < 0)
      smallest = db_iterator_.get();
  }

  current_ = smallest;
}

void TransactionalLevelDBTransaction::TransactionIterator::
    SetCurrentIteratorToLargestKey() {
  TransactionalLevelDBIterator* largest = nullptr;

  if (data_iterator_->IsValid())
    largest = data_iterator_.get();

  if (db_iterator_->IsValid()) {
    if (!largest ||
        comparator_->Compare(db_iterator_->Key(), largest->Key()) > 0)
      largest = db_iterator_.get();
  }

  current_ = largest;
}

void TransactionalLevelDBTransaction::RegisterIterator(
    TransactionIterator* iterator) {
  DCHECK(iterators_.find(iterator) == iterators_.end());
  iterators_.insert(iterator);
}

void TransactionalLevelDBTransaction::UnregisterIterator(
    TransactionIterator* iterator) {
  DCHECK(iterators_.find(iterator) != iterators_.end());
  iterators_.erase(iterator);
}

void TransactionalLevelDBTransaction::NotifyIterators() {
  for (auto* transaction_iterator : iterators_)
    transaction_iterator->DataChanged();
}

std::unique_ptr<LevelDBDirectTransaction> LevelDBDirectTransaction::Create(
    TransactionalLevelDBDatabase* db) {
  return base::WrapUnique(new LevelDBDirectTransaction(db));
}

LevelDBDirectTransaction::LevelDBDirectTransaction(
    TransactionalLevelDBDatabase* db)
    : db_(db), write_batch_(LevelDBWriteBatch::Create()) {}

LevelDBDirectTransaction::~LevelDBDirectTransaction() {
  write_batch_->Clear();
}

void LevelDBDirectTransaction::Put(const StringPiece& key,
                                   const std::string* value) {
  DCHECK(!finished_);
  write_batch_->Put(key, *value);
}

leveldb::Status LevelDBDirectTransaction::Get(const StringPiece& key,
                                              std::string* value,
                                              bool* found) {
  *found = false;
  DCHECK(!finished_);

  leveldb::Status s = db_->Get(key, value, found);
  DCHECK(s.ok() || !*found);
  return s;
}

void LevelDBDirectTransaction::Remove(const StringPiece& key) {
  DCHECK(!finished_);
  write_batch_->Remove(key);
}

leveldb::Status LevelDBDirectTransaction::Commit() {
  DCHECK(!finished_);
  IDB_TRACE("LevelDBDirectTransaction::Commit");

  leveldb::Status s = db_->Write(*write_batch_);
  if (s.ok()) {
    finished_ = true;
    write_batch_->Clear();
  }
  return s;
}

}  // namespace content
