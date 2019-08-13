// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_connection_coordinator.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_callback_helpers.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"
#include "content/browser/indexed_db/indexed_db_origin_state.h"
#include "content/browser/indexed_db/leveldb/leveldb_env.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_database.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_transaction.h"
#include "content/browser/indexed_db/scopes/leveldb_scope.h"
#include "content/browser/indexed_db/scopes/leveldb_scopes.h"
#include "content/browser/indexed_db/scopes/scopes_lock_manager.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using base::ASCIIToUTF16;
using base::NumberToString16;
using blink::IndexedDBDatabaseMetadata;
using leveldb::Status;

namespace content {
// This represents what script calls an 'IDBOpenDBRequest' - either a database
// open or delete call. These may be blocked on other connections. After every
// callback, the request must call
// IndexedDBConnectionCoordinator::RequestComplete() or be expecting a further
// callback.
class IndexedDBConnectionCoordinator::ConnectionRequest {
 public:
  ConnectionRequest(IndexedDBOriginStateHandle origin_state_handle,
                    IndexedDBDatabase* db,
                    IndexedDBConnectionCoordinator* connection_coordinator)
      : origin_state_handle_(std::move(origin_state_handle)),
        db_(db),
        connection_coordinator_(connection_coordinator) {}

  virtual ~ConnectionRequest() {}

  // Called when the request makes it to the front of the queue.
  virtual void Perform() = 0;

  // Called if a front-end signals that it is ignoring a "versionchange"
  // event. This should result in firing a "blocked" event at the request.
  virtual void OnVersionChangeIgnored() const = 0;

  // Called when a connection is closed; if it corresponds to this connection,
  // need to do cleanup. Otherwise, it may unblock further steps.
  // |connection| can be null if all connections were closed (see ForceClose).
  virtual void OnConnectionClosed(IndexedDBConnection* connection) = 0;

  // Called when the transaction should be bound.
  virtual void CreateAndBindTransaction() = 0;

  // Called when the upgrade transaction has started executing.
  virtual void UpgradeTransactionStarted(int64_t old_version) = 0;

  // Called when the upgrade transaction has finished.
  virtual void UpgradeTransactionFinished(bool committed) = 0;

  // Called for pending tasks that we need to clear for a force close. Returns
  // if the request should still execute after all the connections are
  // removed. This is not called for the active request - instead,
  // OnConnectionClosed is called with a nullptr |connection|.
  virtual bool OnForceClose() = 0;

 protected:
  IndexedDBOriginStateHandle origin_state_handle_;
  // This is safe because IndexedDBDatabase owns this object.
  IndexedDBDatabase* db_;

  // Rawptr safe because IndexedDBConnectionCoordinator owns this object.
  IndexedDBConnectionCoordinator* connection_coordinator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionRequest);
};

class IndexedDBConnectionCoordinator::OpenRequest
    : public IndexedDBConnectionCoordinator::ConnectionRequest {
 public:
  OpenRequest(IndexedDBOriginStateHandle origin_state_handle,
              IndexedDBDatabase* db,
              std::unique_ptr<IndexedDBPendingConnection> pending_connection,
              IndexedDBConnectionCoordinator* connection_coordinator)
      : ConnectionRequest(std::move(origin_state_handle),
                          db,
                          connection_coordinator),
        pending_(std::move(pending_connection)) {
    db_->metadata_.was_cold_open = pending_->was_cold_open;
  }

  void Perform() override {
    if (db_->metadata_.id == kInvalidId) {
      // The database was deleted then immediately re-opened; OpenInternal()
      // recreates it in the backing store.
      if (!db_->OpenInternal().ok()) {
        // TODO(jsbell): Consider including sanitized leveldb status message.
        base::string16 message;
        if (pending_->version == IndexedDBDatabaseMetadata::NO_VERSION) {
          message = ASCIIToUTF16(
              "Internal error opening database with no version specified.");
        } else {
          message =
              ASCIIToUTF16("Internal error opening database with version ") +
              NumberToString16(pending_->version);
        }
        pending_->callbacks->OnError(IndexedDBDatabaseError(
            blink::kWebIDBDatabaseExceptionUnknownError, message));
        connection_coordinator_->RequestComplete(this);
        return;
      }

      DCHECK_EQ(IndexedDBDatabaseMetadata::NO_VERSION, db_->metadata_.version);
    }

    const int64_t old_version = db_->metadata_.version;
    int64_t& new_version = pending_->version;

    bool is_new_database = old_version == IndexedDBDatabaseMetadata::NO_VERSION;

    if (new_version == IndexedDBDatabaseMetadata::DEFAULT_VERSION) {
      // For unit tests only - skip upgrade steps. (Calling from script with
      // DEFAULT_VERSION throws exception.)
      DCHECK(is_new_database);
      pending_->callbacks->OnSuccess(
          db_->CreateConnection(std::move(origin_state_handle_),
                                pending_->database_callbacks,
                                pending_->child_process_id),
          db_->metadata_);
      connection_coordinator_->RequestComplete(this);
      return;
    }

    if (!is_new_database &&
        (new_version == old_version ||
         new_version == IndexedDBDatabaseMetadata::NO_VERSION)) {
      pending_->callbacks->OnSuccess(
          db_->CreateConnection(std::move(origin_state_handle_),
                                pending_->database_callbacks,
                                pending_->child_process_id),
          db_->metadata_);
      connection_coordinator_->RequestComplete(this);
      return;
    }

    if (new_version == IndexedDBDatabaseMetadata::NO_VERSION) {
      // If no version is specified and no database exists, upgrade the
      // database version to 1.
      DCHECK(is_new_database);
      new_version = 1;
    } else if (new_version < old_version) {
      // Requested version is lower than current version - fail the request.
      DCHECK(!is_new_database);
      pending_->callbacks->OnError(IndexedDBDatabaseError(
          blink::kWebIDBDatabaseExceptionVersionError,
          ASCIIToUTF16("The requested version (") +
              NumberToString16(pending_->version) +
              ASCIIToUTF16(") is less than the existing version (") +
              NumberToString16(db_->metadata_.version) + ASCIIToUTF16(").")));
      connection_coordinator_->RequestComplete(this);
      return;
    }

    // Requested version is higher than current version - upgrade needed.
    DCHECK_GT(new_version, old_version);

    if (db_->HasNoConnections()) {
      std::vector<ScopesLockManager::ScopeLockRequest> lock_requests = {
          {kDatabaseRangeLockLevel, GetDatabaseLockRange(db_->metadata_.id),
           ScopesLockManager::LockType::kExclusive}};
      db_->lock_manager_->AcquireLocks(
          std::move(lock_requests), lock_receiver_.weak_factory.GetWeakPtr(),
          base::BindOnce(
              &IndexedDBConnectionCoordinator::OpenRequest::StartUpgrade,
              weak_factory_.GetWeakPtr()));
      return;
    }

    // There are outstanding connections - fire "versionchange" events and
    // wait for the connections to close. Front end ensures the event is not
    // fired at connections that have close_pending set. A "blocked" event
    // will be fired at the request when one of the connections acks that the
    // "versionchange" event was ignored.
    DCHECK_NE(pending_->data_loss_info.status,
              blink::mojom::IDBDataLoss::Total);
    db_->SendVersionChangeToAllConnections(old_version, new_version);

    // When all connections have closed the upgrade can proceed.
  }

  void OnVersionChangeIgnored() const override {
    pending_->callbacks->OnBlocked(db_->metadata_.version);
  }

  void OnConnectionClosed(IndexedDBConnection* connection) override {
    if (!connection) {
      pending_->callbacks->OnError(
          IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionAbortError,
                                 "All connections were closed."));
      connection_coordinator_->RequestComplete(this);
      return;
    }
    // This connection closed prematurely; signal an error and complete.
    if (connection && connection->callbacks() == pending_->database_callbacks) {
      pending_->callbacks->OnError(
          IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionAbortError,
                                 "The connection was closed."));
      connection_coordinator_->RequestComplete(this);
      return;
    }

    if (!db_->HasNoConnections())
      return;

    std::vector<ScopesLockManager::ScopeLockRequest> lock_requests = {
        {kDatabaseRangeLockLevel, GetDatabaseLockRange(db_->metadata_.id),
         ScopesLockManager::LockType::kExclusive}};
    db_->lock_manager_->AcquireLocks(
        std::move(lock_requests), lock_receiver_.weak_factory.GetWeakPtr(),
        base::BindOnce(
            &IndexedDBConnectionCoordinator::OpenRequest::StartUpgrade,
            weak_factory_.GetWeakPtr()));
  }

  // Initiate the upgrade. The bulk of the work actually happens in
  // IndexedDBConnectionCoordinator::VersionChangeOperation in order to kick the
  // transaction into the correct state.
  void StartUpgrade() {
    DCHECK(!lock_receiver_.locks.empty());
    connection_ = db_->CreateConnection(std::move(origin_state_handle_),
                                        pending_->database_callbacks,
                                        pending_->child_process_id);
    DCHECK_EQ(connection_coordinator_->connections_.count(connection_.get()),
              1UL);

    std::vector<int64_t> object_store_ids;

    IndexedDBTransaction* transaction = connection_->CreateTransaction(
        pending_->transaction_id,
        std::set<int64_t>(object_store_ids.begin(), object_store_ids.end()),
        blink::mojom::IDBTransactionMode::VersionChange,
        new IndexedDBBackingStore::Transaction(db_->backing_store()));

    // Save a WeakPtr<IndexedDBTransaction> for the CreateAndBindTransaction
    // function to use later.
    pending_->transaction = transaction->AsWeakPtr();

    transaction->ScheduleTask(BindWeakOperation(
        &IndexedDBDatabase::VersionChangeOperation, db_->AsWeakPtr(),
        pending_->version, pending_->callbacks));
    transaction->locks_receiver()->locks = std::move(lock_receiver_.locks);
    transaction->Start();
  }

  void CreateAndBindTransaction() override {
    if (pending_->create_transaction_callback && pending_->transaction) {
      std::move(pending_->create_transaction_callback)
          .Run(std::move(pending_->transaction));
    }
  }

  // Called when the upgrade transaction has started executing.
  void UpgradeTransactionStarted(int64_t old_version) override {
    DCHECK(connection_);
    pending_->callbacks->OnUpgradeNeeded(old_version, std::move(connection_),
                                         db_->metadata_,
                                         pending_->data_loss_info);
  }

  void UpgradeTransactionFinished(bool committed) override {
    // Ownership of connection was already passed along in OnUpgradeNeeded.
    if (committed) {
      DCHECK_EQ(pending_->version, db_->metadata_.version);
      pending_->callbacks->OnSuccess(nullptr, db_->metadata());
    } else {
      DCHECK_NE(pending_->version, db_->metadata_.version);
      pending_->callbacks->OnError(
          IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionAbortError,
                                 "Version change transaction was aborted in "
                                 "upgradeneeded event handler."));
    }
    connection_coordinator_->RequestComplete(this);
  }

  bool OnForceClose() override {
    DCHECK(!connection_);
    pending_->database_callbacks->OnForcedClose();
    pending_.reset();
    return false;
  }

 private:
  ScopesLocksHolder lock_receiver_;

  std::unique_ptr<IndexedDBPendingConnection> pending_;

  // If an upgrade is needed, holds the pending connection until ownership is
  // transferred to the IndexedDBDispatcherHost via OnUpgradeNeeded.
  std::unique_ptr<IndexedDBConnection> connection_;

  base::WeakPtrFactory<OpenRequest> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(OpenRequest);
};

class IndexedDBConnectionCoordinator::DeleteRequest
    : public IndexedDBConnectionCoordinator::ConnectionRequest {
 public:
  DeleteRequest(IndexedDBOriginStateHandle origin_state_handle,
                IndexedDBDatabase* db,
                scoped_refptr<IndexedDBCallbacks> callbacks,
                base::OnceClosure on_database_deleted,
                IndexedDBConnectionCoordinator* connection_coordinator)
      : ConnectionRequest(std::move(origin_state_handle),
                          db,
                          connection_coordinator),
        callbacks_(callbacks),
        on_database_deleted_(std::move(on_database_deleted)) {}

  void Perform() override {
    if (db_->HasNoConnections()) {
      // No connections, so delete immediately.
      std::vector<ScopesLockManager::ScopeLockRequest> lock_requests = {
          {kDatabaseRangeLockLevel, GetDatabaseLockRange(db_->metadata_.id),
           ScopesLockManager::LockType::kExclusive}};
      db_->lock_manager_->AcquireLocks(
          std::move(lock_requests), lock_receiver_.AsWeakPtr(),
          base::BindOnce(
              &IndexedDBConnectionCoordinator::DeleteRequest::DoDelete,
              weak_factory_.GetWeakPtr()));
      return;
    }

    // Front end ensures the event is not fired at connections that have
    // close_pending set.
    const int64_t old_version = db_->metadata_.version;
    const int64_t new_version = IndexedDBDatabaseMetadata::NO_VERSION;
    db_->SendVersionChangeToAllConnections(old_version, new_version);
  }

  void OnVersionChangeIgnored() const override {
    callbacks_->OnBlocked(db_->metadata_.version);
  }

  void OnConnectionClosed(IndexedDBConnection* connection) override {
    if (!db_->HasNoConnections())
      return;

    std::vector<ScopesLockManager::ScopeLockRequest> lock_requests = {
        {kDatabaseRangeLockLevel, GetDatabaseLockRange(db_->metadata_.id),
         ScopesLockManager::LockType::kExclusive}};
    db_->lock_manager_->AcquireLocks(
        std::move(lock_requests), lock_receiver_.AsWeakPtr(),
        base::BindOnce(&IndexedDBConnectionCoordinator::DeleteRequest::DoDelete,
                       weak_factory_.GetWeakPtr()));
  }

  void DoDelete() {
    Status s;
    if (db_->backing_store_) {
      scoped_refptr<TransactionalLevelDBTransaction> txn;
      TransactionalLevelDBDatabase* db = db_->backing_store_->db();
      if (db) {
        txn = db->leveldb_class_factory()->CreateLevelDBTransaction(
            db, db->scopes()->CreateScope(std::move(lock_receiver_.locks), {}));
        txn->set_commit_cleanup_complete_callback(
            std::move(on_database_deleted_));
      }
      s = db_->backing_store_->DeleteDatabase(db_->metadata_.name, txn.get());
    }
    if (!s.ok()) {
      // TODO(jsbell): Consider including sanitized leveldb status message.
      IndexedDBDatabaseError error(blink::kWebIDBDatabaseExceptionUnknownError,
                                   "Internal error deleting database.");
      callbacks_->OnError(error);

      // Calling |error_callback_| can destroy the database and connection
      // coordinator, so keep a WeakPtr to avoid a UAF.
      base::WeakPtr<IndexedDBConnectionCoordinator> connection_coordinator =
          connection_coordinator_->AsWeakPtr();
      db_->error_callback_.Run(s, "Internal error deleting database.");
      if (connection_coordinator)
        connection_coordinator->RequestComplete(this);
      return;
    }

    int64_t old_version = db_->metadata_.version;
    db_->metadata_.id = kInvalidId;
    db_->metadata_.version = IndexedDBDatabaseMetadata::NO_VERSION;
    db_->metadata_.max_object_store_id = kInvalidId;
    db_->metadata_.object_stores.clear();
    // Unittests (specifically the IndexedDBDatabase unittests) can have the
    // backing store be a nullptr, so report deleted here.
    if (on_database_deleted_)
      std::move(on_database_deleted_).Run();
    callbacks_->OnSuccess(old_version);

    connection_coordinator_->RequestComplete(this);
  }

  void CreateAndBindTransaction() override { NOTREACHED(); }

  void UpgradeTransactionStarted(int64_t old_version) override { NOTREACHED(); }

  void UpgradeTransactionFinished(bool committed) override { NOTREACHED(); }

  // The delete requests should always be run during force close.
  bool OnForceClose() override { return true; }

 private:
  ScopesLocksHolder lock_receiver_;
  scoped_refptr<IndexedDBCallbacks> callbacks_;
  base::OnceClosure on_database_deleted_;

  base::WeakPtrFactory<DeleteRequest> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DeleteRequest);
};

IndexedDBConnectionCoordinator::IndexedDBConnectionCoordinator(
    IndexedDBDatabase* db)
    : db_(db) {}
IndexedDBConnectionCoordinator::~IndexedDBConnectionCoordinator() = default;

void IndexedDBConnectionCoordinator::ScheduleOpenConnection(
    IndexedDBOriginStateHandle origin_state_handle,
    std::unique_ptr<IndexedDBPendingConnection> connection) {
  AppendRequest(std::make_unique<OpenRequest>(
      std::move(origin_state_handle), db_, std::move(connection), this));
}

void IndexedDBConnectionCoordinator::ScheduleDeleteDatabase(
    IndexedDBOriginStateHandle origin_state_handle,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    base::OnceClosure on_deletion_complete) {
  AppendRequest(std::make_unique<DeleteRequest>(
      std::move(origin_state_handle), db_, callbacks,
      std::move(on_deletion_complete), this));
}

void IndexedDBConnectionCoordinator::ForceClose() {
  force_closing_ = true;
  // Remove all pending requests that don't want to execute during force close
  // (open requests).
  base::queue<std::unique_ptr<ConnectionRequest>> requests_to_still_run;
  while (!pending_requests_.empty()) {
    std::unique_ptr<ConnectionRequest> request =
        std::move(pending_requests_.front());
    pending_requests_.pop();
    if (request->OnForceClose())
      requests_to_still_run.push(std::move(request));
  }

  if (!requests_to_still_run.empty())
    pending_requests_ = std::move(requests_to_still_run);

  // Since |force_closing_| is true, there are no re-entry modifications to
  // this list by ConnectionClosed().
  while (!connections_.empty()) {
    IndexedDBConnection* connection = *connections_.begin();
    connection->CloseAndReportForceClose();
    connections_.erase(connection);
  }
  force_closing_ = false;

  // OnConnectionClosed usually synchronously calls RequestComplete.
  if (active_request_)
    active_request_->OnConnectionClosed(nullptr);
  else
    db_->ProcessRequestQueueAndMaybeRelease();
}

void IndexedDBConnectionCoordinator::OnConnectionClosed(
    IndexedDBConnection* connection) {
  if (force_closing_)
    return;
  DCHECK(connections().count(connection));
  DCHECK(connection->IsConnected());
  DCHECK(connection->transactions().empty());
  DCHECK(connection->database().get() == db_);

  // Abort transactions before removing the connection; aborting may complete
  // an upgrade, and thus allow the next open/delete requests to proceed. The
  // new active_request_ should see the old connection count until explicitly
  // notified below.
  connections_.erase(connection);

  base::WeakPtr<IndexedDBDatabase> database = db_->AsWeakPtr();
  // Notify the active request, which may need to do cleanup or proceed with
  // the operation. This may trigger other work, such as more connections or
  // deletions, so |active_request_| itself may change.
  if (active_request_)
    active_request_->OnConnectionClosed(connection);

  if (database)
    database->ProcessRequestQueueAndMaybeRelease();
}

void IndexedDBConnectionCoordinator::OnVersionChangeIgnored() {
  DCHECK(active_request_);
  active_request_->OnVersionChangeIgnored();
}

void IndexedDBConnectionCoordinator::OnUpgradeTransactionStarted(
    int64_t old_version) {
  DCHECK(active_request_);
  active_request_->UpgradeTransactionStarted(old_version);
}

void IndexedDBConnectionCoordinator::CreateAndBindUpgradeTransaction() {
  DCHECK(active_request_);
  active_request_->CreateAndBindTransaction();
}

void IndexedDBConnectionCoordinator::OnUpgradeTransactionFinished(
    bool committed) {
  if (active_request_ && !force_closing_)
    active_request_->UpgradeTransactionFinished(committed);
}

void IndexedDBConnectionCoordinator::ProcessRequestQueue() {
  // Don't run re-entrantly to avoid exploding call stacks for requests that
  // complete synchronously. The loop below will process requests until one is
  // blocked.
  if (processing_pending_requests_)
    return;
  processing_pending_requests_ = true;
  // If the active request completed synchronously, keep going.
  while (!active_request_ && !pending_requests_.empty()) {
    active_request_ = std::move(pending_requests_.front());
    pending_requests_.pop();
    active_request_->Perform();
  }
  processing_pending_requests_ = false;
}

void IndexedDBConnectionCoordinator::AddConnection(
    IndexedDBConnection* connection) {
  connections_.insert(connection);
}

void IndexedDBConnectionCoordinator::AppendRequest(
    std::unique_ptr<ConnectionRequest> request) {
  pending_requests_.push(std::move(request));

  // This may be an unrelated transaction finishing while waiting for
  // connections to close, or the actual upgrade transaction from an active
  // request. Notify the active request if it's the latter.
  if (!active_request_)
    db_->ProcessRequestQueueAndMaybeRelease();
}

void IndexedDBConnectionCoordinator::RequestComplete(
    ConnectionRequest* request) {
  DCHECK_EQ(request, active_request_.get());
  // Destroying a request can cause this instance to be destroyed (through
  // ConnectionClosed), so hold a WeakPtr.
  base::WeakPtr<IndexedDBDatabase> weak_ptr = db_->AsWeakPtr();
  active_request_.reset();

  if (!weak_ptr)
    return;

  db_->ProcessRequestQueueAndMaybeRelease();
}

}  // namespace content
