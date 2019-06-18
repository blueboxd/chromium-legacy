// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_connection.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_observer.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"

namespace content {

namespace {

static int32_t g_next_indexed_db_connection_id;

}  // namespace

IndexedDBConnection::IndexedDBConnection(
    int child_process_id,
    IndexedDBOriginStateHandle origin_state_handle,
    IndexedDBClassFactory* indexed_db_class_factory,
    base::WeakPtr<IndexedDBDatabase> database,
    base::RepeatingClosure on_version_change_ignored,
    base::OnceCallback<void(IndexedDBConnection*)> on_close,
    ErrorCallback error_callback,
    scoped_refptr<IndexedDBDatabaseCallbacks> callbacks)
    : id_(g_next_indexed_db_connection_id++),
      child_process_id_(child_process_id),
      origin_state_handle_(std::move(origin_state_handle)),
      indexed_db_class_factory_(indexed_db_class_factory),
      database_(std::move(database)),
      on_version_change_ignored_(std::move(on_version_change_ignored)),
      on_close_(std::move(on_close)),
      error_callback_(std::move(error_callback)),
      callbacks_(callbacks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

IndexedDBConnection::~IndexedDBConnection() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (on_close_)
    std::move(on_close_).Run(this);
}

void IndexedDBConnection::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callbacks_.get())
    return;

  // Finish up any transaction, in case there were any running.
  FinishAllTransactions(IndexedDBDatabaseError(
      blink::kWebIDBDatabaseExceptionUnknownError, "Connection is closing."));

  // Calling |on_close_| can destroy this object.
  base::WeakPtr<IndexedDBConnection> this_obj = weak_factory_.GetWeakPtr();
  std::move(on_close_).Run(this);
  if (this_obj)
    ClearStateAfterClose();
}

void IndexedDBConnection::CloseAndReportForceClose() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!callbacks_.get())
    return;

  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks(callbacks_);
  Close();
  callbacks->OnForcedClose();
}

void IndexedDBConnection::VersionChangeIgnored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  on_version_change_ignored_.Run();
}

bool IndexedDBConnection::IsConnected() {
  return callbacks_.get();
}

// The observers begin listening to changes only once they are activated.
void IndexedDBConnection::ActivatePendingObservers(
    std::vector<std::unique_ptr<IndexedDBObserver>> pending_observers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& observer : pending_observers) {
    active_observers_.push_back(std::move(observer));
  }
  pending_observers.clear();
}

void IndexedDBConnection::RemoveObservers(
    const std::vector<int32_t>& observer_ids_to_remove) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<int32_t> pending_observer_ids;
  for (int32_t id_to_remove : observer_ids_to_remove) {
    const auto& it = std::find_if(
        active_observers_.begin(), active_observers_.end(),
        [&id_to_remove](const std::unique_ptr<IndexedDBObserver>& o) {
          return o->id() == id_to_remove;
        });
    if (it != active_observers_.end())
      active_observers_.erase(it);
    else
      pending_observer_ids.push_back(id_to_remove);
  }
  if (pending_observer_ids.empty())
    return;

  for (const auto& it : transactions_) {
    it.second->RemovePendingObservers(pending_observer_ids);
  }
}

IndexedDBTransaction* IndexedDBConnection::CreateTransaction(
    int64_t id,
    const std::set<int64_t>& scope,
    blink::mojom::IDBTransactionMode mode,
    IndexedDBBackingStore::Transaction* backing_store_transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(GetTransaction(id), nullptr) << "Duplicate transaction id." << id;
  std::unique_ptr<IndexedDBTransaction> transaction =
      indexed_db_class_factory_->CreateIndexedDBTransaction(
          id, this, error_callback_, scope, mode, backing_store_transaction);
  IndexedDBTransaction* transaction_ptr = transaction.get();
  transactions_[id] = std::move(transaction);
  return transaction_ptr;
}

void IndexedDBConnection::AbortTransaction(
    IndexedDBTransaction* transaction,
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  IDB_TRACE1("IndexedDBDatabase::Abort(error)", "txn.id", transaction->id());
  transaction->Abort(error);
}

void IndexedDBConnection::FinishAllTransactions(
    const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unordered_map<int64_t, std::unique_ptr<IndexedDBTransaction>> temp_map;
  std::swap(temp_map, transactions_);
  for (const auto& pair : temp_map) {
    auto& transaction = pair.second;
    if (transaction->is_commit_pending()) {
      IDB_TRACE1("IndexedDBDatabase::Commit", "transaction.id",
                 transaction->id());
      transaction->ForcePendingCommit();
    } else {
      IDB_TRACE1("IndexedDBDatabase::Abort(error)", "transaction.id",
                 transaction->id());
      transaction->Abort(error);
    }
  }
}

IndexedDBTransaction* IndexedDBConnection::GetTransaction(int64_t id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = transactions_.find(id);
  if (it == transactions_.end())
    return nullptr;
  return it->second.get();
}

base::WeakPtr<IndexedDBTransaction>
IndexedDBConnection::AddTransactionForTesting(
    std::unique_ptr<IndexedDBTransaction> transaction) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!base::Contains(transactions_, transaction->id()));
  base::WeakPtr<IndexedDBTransaction> transaction_ptr =
      transaction->ptr_factory_.GetWeakPtr();
  transactions_[transaction->id()] = std::move(transaction);
  return transaction_ptr;
}

void IndexedDBConnection::RemoveTransaction(int64_t id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  transactions_.erase(id);
}

void IndexedDBConnection::ClearStateAfterClose() {
  callbacks_ = nullptr;
  active_observers_.clear();
  origin_state_handle_.Release();
}

}  // namespace content
