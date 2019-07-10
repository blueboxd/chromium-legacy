// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_class_factory.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_iterator_impl.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_transaction.h"

namespace content {

static IndexedDBClassFactory::GetterCallback* s_factory_getter;
static ::base::LazyInstance<IndexedDBClassFactory>::Leaky s_factory =
    LAZY_INSTANCE_INITIALIZER;

void IndexedDBClassFactory::SetIndexedDBClassFactoryGetter(GetterCallback* cb) {
  s_factory_getter = cb;
}

IndexedDBClassFactory* IndexedDBClassFactory::Get() {
  if (s_factory_getter)
    return (*s_factory_getter)();
  else
    return s_factory.Pointer();
}

std::pair<std::unique_ptr<IndexedDBDatabase>, leveldb::Status>
IndexedDBClassFactory::CreateIndexedDBDatabase(
    const base::string16& name,
    IndexedDBBackingStore* backing_store,
    IndexedDBFactory* factory,
    IndexedDBDatabase::ErrorCallback error_callback,
    base::OnceClosure destroy_me,
    std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
    const IndexedDBDatabase::Identifier& unique_identifier,
    ScopesLockManager* transaction_lock_manager) {
  DCHECK(backing_store);
  DCHECK(factory);
  std::unique_ptr<IndexedDBDatabase> database =
      base::WrapUnique(new IndexedDBDatabase(
          name, backing_store, factory, this, std::move(error_callback),
          std::move(destroy_me), std::move(metadata_coding), unique_identifier,
          transaction_lock_manager));
  leveldb::Status s = database->OpenInternal();
  if (!s.ok())
    database = nullptr;
  return {std::move(database), s};
}

std::unique_ptr<IndexedDBTransaction>
IndexedDBClassFactory::CreateIndexedDBTransaction(
    int64_t id,
    IndexedDBConnection* connection,
    ErrorCallback error_callback,
    const std::set<int64_t>& scope,
    blink::mojom::IDBTransactionMode mode,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  return base::WrapUnique(
      new IndexedDBTransaction(id, connection, std::move(error_callback), scope,
                               mode, backing_store_transaction));
}

}  // namespace content
