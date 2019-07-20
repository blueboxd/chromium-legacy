// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/transactional_leveldb_iterator_impl.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_database.h"

static leveldb::Slice MakeSlice(const base::StringPiece& s) {
  return leveldb::Slice(s.begin(), s.size());
}

static base::StringPiece MakeStringPiece(const leveldb::Slice& s) {
  return base::StringPiece(s.data(), s.size());
}

namespace content {

TransactionalLevelDBIteratorImpl::~TransactionalLevelDBIteratorImpl() {
  if (db_)
    db_->OnIteratorDestroyed(this);
}

TransactionalLevelDBIteratorImpl::TransactionalLevelDBIteratorImpl(
    std::unique_ptr<leveldb::Iterator> it,
    TransactionalLevelDBDatabase* db,
    const leveldb::Snapshot* snapshot)
    : iterator_(std::move(it)), db_(db->AsWeakPtr()), snapshot_(snapshot) {}

leveldb::Status TransactionalLevelDBIteratorImpl::CheckStatus() {
  DCHECK(!IsDetached());
  const leveldb::Status& s = iterator_->status();
  if (!s.ok())
    LOG(ERROR) << "LevelDB iterator error: " << s.ToString();
  return s;
}

bool TransactionalLevelDBIteratorImpl::IsValid() const {
  switch (iterator_state_) {
    case IteratorState::EVICTED_AND_VALID:
      return true;
    case IteratorState::EVICTED_AND_INVALID:
      return false;
    case IteratorState::ACTIVE:
      return iterator_->Valid();
  }
  NOTREACHED();
  return false;
}

leveldb::Status TransactionalLevelDBIteratorImpl::SeekToLast() {
  WillUseDBIterator();
  DCHECK(iterator_);
  iterator_->SeekToLast();
  return CheckStatus();
}

leveldb::Status TransactionalLevelDBIteratorImpl::Seek(
    const base::StringPiece& target) {
  WillUseDBIterator();
  DCHECK(iterator_);
  iterator_->Seek(MakeSlice(target));
  return CheckStatus();
}

leveldb::Status TransactionalLevelDBIteratorImpl::Next() {
  DCHECK(IsValid());
  WillUseDBIterator();
  DCHECK(iterator_);
  iterator_->Next();
  return CheckStatus();
}

leveldb::Status TransactionalLevelDBIteratorImpl::Prev() {
  DCHECK(IsValid());
  WillUseDBIterator();
  DCHECK(iterator_);
  iterator_->Prev();
  return CheckStatus();
}

base::StringPiece TransactionalLevelDBIteratorImpl::Key() const {
  DCHECK(IsValid());
  if (IsDetached())
    return key_before_eviction_;
  return MakeStringPiece(iterator_->key());
}

base::StringPiece TransactionalLevelDBIteratorImpl::Value() const {
  DCHECK(IsValid());
  // Always need to update the LRU, so we always call this. Const-cast needed,
  // as we're implementing a caching layer.
  TransactionalLevelDBIteratorImpl* non_const =
      const_cast<TransactionalLevelDBIteratorImpl*>(this);
  non_const->WillUseDBIterator();
  return MakeStringPiece(iterator_->value());
}

void TransactionalLevelDBIteratorImpl::Detach() {
  DCHECK(!IsDetached());
  if (iterator_->Valid()) {
    iterator_state_ = IteratorState::EVICTED_AND_VALID;
    key_before_eviction_ = iterator_->key().ToString();
  } else {
    iterator_state_ = IteratorState::EVICTED_AND_INVALID;
  }
  iterator_.reset();
}

bool TransactionalLevelDBIteratorImpl::IsDetached() const {
  return iterator_state_ != IteratorState::ACTIVE;
}

void TransactionalLevelDBIteratorImpl::WillUseDBIterator() {
  db_->OnIteratorUsed(this);
  if (!IsDetached())
    return;

  iterator_ = db_->CreateLevelDBIterator(snapshot_);
  if (iterator_state_ == IteratorState::EVICTED_AND_VALID) {
    iterator_->Seek(key_before_eviction_);
    key_before_eviction_.clear();
    DCHECK(IsValid());
  } else {
    DCHECK(!iterator_->Valid());
  }
  iterator_state_ = IteratorState::ACTIVE;
}

}  // namespace content
