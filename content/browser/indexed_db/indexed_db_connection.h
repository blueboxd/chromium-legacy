// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_origin_state_handle.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {
class IndexedDBDatabaseCallbacks;
class IndexedDBDatabaseError;
class IndexedDBObserver;
class IndexedDBTransaction;
class IndexedDBOriginStateHandle;

class CONTENT_EXPORT IndexedDBConnection {
 public:
  // Used to report irrecoverable backend errors. The second argument can be
  // null.
  using ErrorCallback =
      base::RepeatingCallback<void(leveldb::Status, const char*)>;

  IndexedDBConnection(int child_process_id,
                      IndexedDBOriginStateHandle origin_state_handle,
                      IndexedDBClassFactory* indexed_db_class_factory,
                      base::WeakPtr<IndexedDBDatabase> database,
                      base::RepeatingClosure on_version_change_ignored,
                      base::OnceCallback<void(IndexedDBConnection*)> on_close,
                      ErrorCallback error_callback,
                      scoped_refptr<IndexedDBDatabaseCallbacks> callbacks);
  virtual ~IndexedDBConnection();

  void Close();
  void CloseAndReportForceClose();
  bool IsConnected();

  void VersionChangeIgnored();

  virtual void ActivatePendingObservers(
      std::vector<std::unique_ptr<IndexedDBObserver>> pending_observers);
  // Removes observer listed in |remove_observer_ids| from active_observer of
  // connection or pending_observer of transactions associated with this
  // connection.
  virtual void RemoveObservers(const std::vector<int32_t>& remove_observer_ids);

  int32_t id() const { return id_; }
  int child_process_id() const { return child_process_id_; }

  base::WeakPtr<IndexedDBDatabase> database() const { return database_; }
  IndexedDBDatabaseCallbacks* callbacks() const { return callbacks_.get(); }
  const std::vector<std::unique_ptr<IndexedDBObserver>>& active_observers()
      const {
    return active_observers_;
  }
  base::WeakPtr<IndexedDBConnection> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // Creates a transaction for this connection.
  IndexedDBTransaction* CreateTransaction(
      int64_t id,
      const std::set<int64_t>& scope,
      blink::mojom::IDBTransactionMode mode,
      IndexedDBBackingStore::Transaction* backing_store_transaction);

  void AbortTransaction(IndexedDBTransaction* transaction,
                        const IndexedDBDatabaseError& error);

  // Aborts or commits each transaction owned by this connection depending on
  // the transaction's current state. Any transaction with is_commit_pending_
  // false is aborted, and any transaction with is_commit_pending_ true is
  // committed.
  void FinishAllTransactions(const IndexedDBDatabaseError& error);

  IndexedDBTransaction* GetTransaction(int64_t id) const;

  base::WeakPtr<IndexedDBTransaction> AddTransactionForTesting(
      std::unique_ptr<IndexedDBTransaction> transaction);

  // We ignore calls where the id doesn't exist to facilitate the AbortAll call.
  // TODO(dmurph): Change that so this doesn't need to ignore unknown ids.
  void RemoveTransaction(int64_t id);

  const std::unordered_map<int64_t, std::unique_ptr<IndexedDBTransaction>>&
  transactions() const {
    return transactions_;
  }

 private:
  void ClearStateAfterClose();

  const int32_t id_;

  // The process id of the child process this connection is associated with.
  // Tracked for IndexedDBContextImpl::GetAllOriginsDetails and debugging.
  const int child_process_id_;

  // Keeps the factory for this origin alive.
  IndexedDBOriginStateHandle origin_state_handle_;
  IndexedDBClassFactory* const indexed_db_class_factory_;

  base::WeakPtr<IndexedDBDatabase> database_;
  base::RepeatingClosure on_version_change_ignored_;
  // Note: Calling |on_close_| can destroy this object.
  base::OnceCallback<void(IndexedDBConnection*)> on_close_;
  ErrorCallback error_callback_;

  // The connection owns transactions created on this connection.
  std::unordered_map<int64_t, std::unique_ptr<IndexedDBTransaction>>
      transactions_;

  // The callbacks_ member is cleared when the connection is closed.
  // May be NULL in unit tests.
  scoped_refptr<IndexedDBDatabaseCallbacks> callbacks_;
  std::vector<std::unique_ptr<IndexedDBObserver>> active_observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IndexedDBConnection> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBConnection);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_H_
