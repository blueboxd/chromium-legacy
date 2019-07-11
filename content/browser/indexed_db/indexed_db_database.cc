// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_database.h"

#include <math.h>
#include <algorithm>
#include <limits>
#include <set>
#include <utility>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_blob_info.h"
#include "content/browser/indexed_db/indexed_db_callback_helpers.h"
#include "content/browser/indexed_db/indexed_db_class_factory.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_factory.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_index_writer.h"
#include "content/browser/indexed_db/indexed_db_metadata_coding.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/browser/indexed_db/scopes/scope_lock.h"
#include "content/browser/indexed_db/scopes/scopes_lock_manager.h"
#include "content/browser/indexed_db/transaction_impl.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_channel.h"
#include "storage/browser/blob/blob_data_handle.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_path.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key_range.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "url/origin.h"

using base::ASCIIToUTF16;
using base::NumberToString16;
using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBIndexKeys;
using blink::IndexedDBIndexMetadata;
using blink::IndexedDBKey;
using blink::IndexedDBKeyPath;
using blink::IndexedDBKeyRange;
using blink::IndexedDBObjectStoreMetadata;
using leveldb::Status;

namespace content {
namespace {

// Used for WebCore.IndexedDB.Schema.ObjectStore.KeyPathType and
// WebCore.IndexedDB.Schema.Index.KeyPathType histograms. Do not
// modify (delete, re-order, renumber) these values other than
// the _MAX value.
enum HistogramIDBKeyPathType {
  KEY_PATH_TYPE_NONE = 0,
  KEY_PATH_TYPE_STRING = 1,
  KEY_PATH_TYPE_ARRAY = 2,
  KEY_PATH_TYPE_MAX = 3,  // Keep as last/max entry, for histogram range.
};

HistogramIDBKeyPathType HistogramKeyPathType(const IndexedDBKeyPath& key_path) {
  switch (key_path.type()) {
    case blink::mojom::IDBKeyPathType::Null:
      return KEY_PATH_TYPE_NONE;
    case blink::mojom::IDBKeyPathType::String:
      return KEY_PATH_TYPE_STRING;
    case blink::mojom::IDBKeyPathType::Array:
      return KEY_PATH_TYPE_ARRAY;
  }
  NOTREACHED();
  return KEY_PATH_TYPE_NONE;
}

IndexedDBDatabaseError CreateError(uint16_t code,
                                   const char* message,
                                   IndexedDBTransaction* transaction) {
  transaction->IncrementNumErrorsSent();
  return IndexedDBDatabaseError(code, message);
}

IndexedDBDatabaseError CreateError(uint16_t code,
                                   const base::string16& message,
                                   IndexedDBTransaction* transaction) {
  transaction->IncrementNumErrorsSent();
  return IndexedDBDatabaseError(code, message);
}

}  // namespace

// This represents what script calls an 'IDBOpenDBRequest' - either a database
// open or delete call. These may be blocked on other connections. After every
// callback, the request must call IndexedDBDatabase::RequestComplete() or be
// expecting a further callback.
class IndexedDBDatabase::ConnectionRequest {
 public:
  ConnectionRequest(IndexedDBOriginStateHandle origin_state_handle,
                    IndexedDBDatabase* db)
      : origin_state_handle_(std::move(origin_state_handle)),
        db_(std::move(db)) {}

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

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionRequest);
};

class IndexedDBDatabase::OpenRequest
    : public IndexedDBDatabase::ConnectionRequest {
 public:
  OpenRequest(IndexedDBOriginStateHandle origin_state_handle,
              IndexedDBDatabase* db,
              std::unique_ptr<IndexedDBPendingConnection> pending_connection)
      : ConnectionRequest(std::move(origin_state_handle), db),
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
        db_->RequestComplete(this);
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
      db_->RequestComplete(this);
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
      db_->RequestComplete(this);
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
      db_->RequestComplete(this);
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
          base::BindOnce(&IndexedDBDatabase::OpenRequest::StartUpgrade,
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
      db_->RequestComplete(this);
      return;
    }
    // This connection closed prematurely; signal an error and complete.
    if (connection && connection->callbacks() == pending_->database_callbacks) {
      pending_->callbacks->OnError(
          IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionAbortError,
                                 "The connection was closed."));
      db_->RequestComplete(this);
      return;
    }

    if (!db_->HasNoConnections())
      return;

    std::vector<ScopesLockManager::ScopeLockRequest> lock_requests = {
        {kDatabaseRangeLockLevel, GetDatabaseLockRange(db_->metadata_.id),
         ScopesLockManager::LockType::kExclusive}};
    db_->lock_manager_->AcquireLocks(
        std::move(lock_requests), lock_receiver_.weak_factory.GetWeakPtr(),
        base::BindOnce(&IndexedDBDatabase::OpenRequest::StartUpgrade,
                       weak_factory_.GetWeakPtr()));
  }

  // Initiate the upgrade. The bulk of the work actually happens in
  // IndexedDBDatabase::VersionChangeOperation in order to kick the
  // transaction into the correct state.
  void StartUpgrade() {
    DCHECK(!lock_receiver_.locks.empty());
    connection_ = db_->CreateConnection(std::move(origin_state_handle_),
                                        pending_->database_callbacks,
                                        pending_->child_process_id);
    DCHECK_EQ(db_->connections_.count(connection_.get()), 1UL);

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
      pending_->callbacks->OnSuccess(std::unique_ptr<IndexedDBConnection>(),
                                     db_->metadata());
    } else {
      DCHECK_NE(pending_->version, db_->metadata_.version);
      pending_->callbacks->OnError(
          IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionAbortError,
                                 "Version change transaction was aborted in "
                                 "upgradeneeded event handler."));
    }
    db_->RequestComplete(this);
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

class IndexedDBDatabase::DeleteRequest
    : public IndexedDBDatabase::ConnectionRequest {
 public:
  DeleteRequest(IndexedDBOriginStateHandle origin_state_handle,
                IndexedDBDatabase* db,
                scoped_refptr<IndexedDBCallbacks> callbacks,
                base::OnceClosure on_database_deleted)
      : ConnectionRequest(std::move(origin_state_handle), db),
        callbacks_(callbacks),
        on_database_deleted_(std::move(on_database_deleted)) {}

  void Perform() override {
    if (db_->HasNoConnections()) {
      // No connections, so delete immediately.
      DoDelete();
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

    DoDelete();
  }

  void DoDelete() {
    Status s;
    if (db_->backing_store_)
      s = db_->backing_store_->DeleteDatabase(db_->metadata_.name);
    if (!s.ok()) {
      // TODO(jsbell): Consider including sanitized leveldb status message.
      IndexedDBDatabaseError error(blink::kWebIDBDatabaseExceptionUnknownError,
                                   "Internal error deleting database.");
      callbacks_->OnError(error);

      base::WeakPtr<IndexedDBDatabase> database = db_->AsWeakPtr();
      db_->error_callback_.Run(s, "Internal error deleting database.");
      if (database)
        database->RequestComplete(this);
      return;
    }

    int64_t old_version = db_->metadata_.version;
    db_->metadata_.id = kInvalidId;
    db_->metadata_.version = IndexedDBDatabaseMetadata::NO_VERSION;
    db_->metadata_.max_object_store_id = kInvalidId;
    db_->metadata_.object_stores.clear();
    callbacks_->OnSuccess(old_version);
    std::move(on_database_deleted_).Run();

    db_->RequestComplete(this);
  }

  void CreateAndBindTransaction() override { NOTREACHED(); }

  void UpgradeTransactionStarted(int64_t old_version) override { NOTREACHED(); }

  void UpgradeTransactionFinished(bool committed) override { NOTREACHED(); }

  // The delete requests should always be run during force close.
  bool OnForceClose() override { return true; }

 private:
  scoped_refptr<IndexedDBCallbacks> callbacks_;
  base::OnceClosure on_database_deleted_;

  base::WeakPtrFactory<DeleteRequest> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(DeleteRequest);
};

IndexedDBDatabase::IndexedDBDatabase(
    const base::string16& name,
    IndexedDBBackingStore* backing_store,
    IndexedDBFactory* factory,
    IndexedDBClassFactory* class_factory,
    ErrorCallback error_callback,
    base::OnceClosure destroy_me,
    std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
    const Identifier& unique_identifier,
    ScopesLockManager* transaction_lock_manager)
    : backing_store_(backing_store),
      metadata_(name,
                kInvalidId,
                IndexedDBDatabaseMetadata::NO_VERSION,
                kInvalidId),
      identifier_(unique_identifier),
      factory_(factory),
      class_factory_(class_factory),
      metadata_coding_(std::move(metadata_coding)),
      lock_manager_(transaction_lock_manager),
      error_callback_(std::move(error_callback)),
      destroy_me_(std::move(destroy_me)) {
  DCHECK(destroy_me_);
  DCHECK(factory != nullptr);
}

IndexedDBDatabase::~IndexedDBDatabase() {
  DCHECK(!active_request_);
  DCHECK(pending_requests_.empty());
  DCHECK(connections_.empty())
      << "Cannot destroy a database that has connections";
}

void IndexedDBDatabase::AddObjectStore(
    IndexedDBObjectStoreMetadata object_store,
    int64_t new_max_object_store_id) {
  DCHECK(metadata_.object_stores.find(object_store.id) ==
         metadata_.object_stores.end());
  if (new_max_object_store_id != IndexedDBObjectStoreMetadata::kInvalidId) {
    DCHECK_LT(metadata_.max_object_store_id, new_max_object_store_id);
    metadata_.max_object_store_id = new_max_object_store_id;
  }
  metadata_.object_stores[object_store.id] = std::move(object_store);
}

IndexedDBObjectStoreMetadata IndexedDBDatabase::RemoveObjectStore(
    int64_t object_store_id) {
  auto it = metadata_.object_stores.find(object_store_id);
  CHECK(it != metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata metadata = std::move(it->second);
  metadata_.object_stores.erase(it);
  return metadata;
}

void IndexedDBDatabase::AddIndex(int64_t object_store_id,
                                 IndexedDBIndexMetadata index,
                                 int64_t new_max_index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index.id) == object_store.indexes.end());
  object_store.indexes[index.id] = std::move(index);
  if (new_max_index_id != IndexedDBIndexMetadata::kInvalidId) {
    DCHECK_LT(object_store.max_index_id, new_max_index_id);
    object_store.max_index_id = new_max_index_id;
  }
}

IndexedDBIndexMetadata IndexedDBDatabase::RemoveIndex(int64_t object_store_id,
                                                      int64_t index_id) {
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[object_store_id];

  auto it = object_store.indexes.find(index_id);
  CHECK(it != object_store.indexes.end());
  IndexedDBIndexMetadata metadata = std::move(it->second);
  object_store.indexes.erase(it);
  return metadata;
}

Status IndexedDBDatabase::OpenInternal() {
  bool found = false;
  Status s = metadata_coding_->ReadMetadataForDatabaseName(
      backing_store_->db(), backing_store_->origin_identifier(), metadata_.name,
      &metadata_, &found);
  DCHECK(found == (metadata_.id != kInvalidId))
      << "found = " << found << " id = " << metadata_.id;
  if (!s.ok() || found)
    return s;

  return metadata_coding_->CreateDatabase(
      backing_store_->db(), backing_store_->origin_identifier(), metadata_.name,
      metadata_.version, &metadata_);
}

// kIDBMaxMessageSize is defined based on the original
// IPC::Channel::kMaximumMessageSize value.  We use kIDBMaxMessageSize to limit
// the size of arguments we pass into our Mojo calls.  We want to ensure this
// value is always no bigger than the current kMaximumMessageSize value which
// also ensures it is always no bigger than the current Mojo message size limit.
static_assert(
    blink::mojom::kIDBMaxMessageSize <= IPC::Channel::kMaximumMessageSize,
    "kIDBMaxMessageSize is bigger than IPC::Channel::kMaximumMessageSize");

size_t IndexedDBDatabase::GetUsableMessageSizeInBytes() const {
  return blink::mojom::kIDBMaxMessageSize -
         blink::mojom::kIDBMaxMessageOverhead;
}

std::unique_ptr<IndexedDBConnection> IndexedDBDatabase::CreateConnection(
    IndexedDBOriginStateHandle origin_state_handle,
    scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
    int child_process_id) {
  std::unique_ptr<IndexedDBConnection> connection =
      std::make_unique<IndexedDBConnection>(
          child_process_id, std::move(origin_state_handle), class_factory_,
          weak_factory_.GetWeakPtr(),
          base::BindRepeating(&IndexedDBDatabase::VersionChangeIgnored,
                              weak_factory_.GetWeakPtr()),
          base::BindOnce(&IndexedDBDatabase::ConnectionClosed,
                         weak_factory_.GetWeakPtr()),
          error_callback_, database_callbacks);
  connections_.insert(connection.get());
  backing_store_->GrantChildProcessPermissions(child_process_id);
  return connection;
}

void IndexedDBDatabase::VersionChangeIgnored() {
  if (active_request_)
    active_request_->OnVersionChangeIgnored();
}

bool IndexedDBDatabase::HasNoConnections() const {
  return force_closing_ || connections_.empty();
}

void IndexedDBDatabase::SendVersionChangeToAllConnections(int64_t old_version,
                                                          int64_t new_version) {
  if (force_closing_)
    return;
  for (const auto* connection : connections_)
    connection->callbacks()->OnVersionChange(old_version, new_version);
}

void IndexedDBDatabase::ConnectionClosed(IndexedDBConnection* connection) {
  if (force_closing_)
    return;
  DCHECK(connections_.count(connection));
  DCHECK(connection->IsConnected());
  DCHECK(connection->transactions().empty());
  DCHECK(connection->database().get() == this);

  IDB_TRACE("IndexedDBDatabase::Close");

  // Abort transactions before removing the connection; aborting may complete
  // an upgrade, and thus allow the next open/delete requests to proceed. The
  // new active_request_ should see the old connection count until explicitly
  // notified below.
  connections_.erase(connection);

  base::WeakPtr<IndexedDBDatabase> database = weak_factory_.GetWeakPtr();
  // Notify the active request, which may need to do cleanup or proceed with
  // the operation. This may trigger other work, such as more connections or
  // deletions, so |active_request_| itself may change.
  if (active_request_)
    active_request_->OnConnectionClosed(connection);

  if (database)
    ProcessRequestQueueAndMaybeRelease();
}

bool IndexedDBDatabase::ValidateObjectStoreId(int64_t object_store_id) const {
  if (!base::Contains(metadata_.object_stores, object_store_id)) {
    DLOG(ERROR) << "Invalid object_store_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::ValidateObjectStoreIdAndIndexId(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!ValidateObjectStoreId(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (!base::Contains(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::ValidateObjectStoreIdAndOptionalIndexId(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!ValidateObjectStoreId(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (index_id != IndexedDBIndexMetadata::kInvalidId &&
      !base::Contains(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

bool IndexedDBDatabase::ValidateObjectStoreIdAndNewIndexId(
    int64_t object_store_id,
    int64_t index_id) const {
  if (!ValidateObjectStoreId(object_store_id))
    return false;
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores.find(object_store_id)->second;
  if (base::Contains(object_store_metadata.indexes, index_id)) {
    DLOG(ERROR) << "Invalid index_id";
    return false;
  }
  return true;
}

void IndexedDBDatabase::CreateObjectStore(IndexedDBTransaction* transaction,
                                          int64_t object_store_id,
                                          const base::string16& name,
                                          const IndexedDBKeyPath& key_path,
                                          bool auto_increment) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::CreateObjectStore", "txn.id",
             transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (base::Contains(metadata_.object_stores, object_store_id)) {
    DLOG(ERROR) << "Invalid object_store_id";
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("WebCore.IndexedDB.Schema.ObjectStore.KeyPathType",
                            HistogramKeyPathType(key_path), KEY_PATH_TYPE_MAX);
  UMA_HISTOGRAM_BOOLEAN("WebCore.IndexedDB.Schema.ObjectStore.AutoIncrement",
                        auto_increment);

  // Store creation is done synchronously, as it may be followed by
  // index creation (also sync) since preemptive OpenCursor/SetIndexKeys
  // may follow.

  // TODO(dmurph): Remove this call once this method is asynchronous (scheduled
  // on the transaction).
  transaction->EnsureBackingStoreTransactionBegun();

  IndexedDBObjectStoreMetadata object_store_metadata;
  Status s = metadata_coding_->CreateObjectStore(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), object_store_id, name, key_path,
      auto_increment, &object_store_metadata);

  if (!s.ok()) {
    error_callback_.Run(s, "Internal error creating object store.");
    return;
  }

  AddObjectStore(std::move(object_store_metadata), object_store_id);

  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::CreateObjectStoreAbortOperation,
                     AsWeakPtr(), object_store_id));
}

void IndexedDBDatabase::DeleteObjectStore(IndexedDBTransaction* transaction,
                                          int64_t object_store_id) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::DeleteObjectStore", "txn.id",
             transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::DeleteObjectStoreOperation,
                        AsWeakPtr(), object_store_id));
}

void IndexedDBDatabase::RenameObjectStore(IndexedDBTransaction* transaction,
                                          int64_t object_store_id,
                                          const base::string16& new_name) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::RenameObjectStore", "txn.id",
             transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  // Store renaming is done synchronously, as it may be followed by
  // index creation (also sync) since preemptive OpenCursor/SetIndexKeys
  // may follow.
  IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  base::string16 old_name;

  Status s = metadata_coding_->RenameObjectStore(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), new_name, &old_name,
      &object_store_metadata);

  if (!s.ok()) {
    error_callback_.Run(s, "Internal error renaming object store.");
    return;
  }
  DCHECK_EQ(object_store_metadata.name, new_name);

  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::RenameObjectStoreAbortOperation,
                     AsWeakPtr(), object_store_id, std::move(old_name)));
}

void IndexedDBDatabase::CreateIndex(IndexedDBTransaction* transaction,
                                    int64_t object_store_id,
                                    int64_t index_id,
                                    const base::string16& name,
                                    const IndexedDBKeyPath& key_path,
                                    bool unique,
                                    bool multi_entry) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::CreateIndex", "txn.id", transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!ValidateObjectStoreIdAndNewIndexId(object_store_id, index_id))
    return;

  UMA_HISTOGRAM_ENUMERATION("WebCore.IndexedDB.Schema.Index.KeyPathType",
                            HistogramKeyPathType(key_path), KEY_PATH_TYPE_MAX);
  UMA_HISTOGRAM_BOOLEAN("WebCore.IndexedDB.Schema.Index.Unique", unique);
  UMA_HISTOGRAM_BOOLEAN("WebCore.IndexedDB.Schema.Index.MultiEntry",
                        multi_entry);

  // TODO(dmurph): Remove this call once this method is asynchronous (scheduled
  // on the transaction).
  transaction->EnsureBackingStoreTransactionBegun();

  // Index creation is done synchronously since preemptive
  // OpenCursor/SetIndexKeys may follow.
  // TODO(dmurph): Make this asynchronous.
  IndexedDBIndexMetadata index_metadata;
  Status s = metadata_coding_->CreateIndex(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), object_store_id, index_id, name, key_path,
      unique, multi_entry, &index_metadata);

  if (!s.ok()) {
    base::string16 error_string =
        ASCIIToUTF16("Internal error creating index '") + index_metadata.name +
        ASCIIToUTF16("'.");
    transaction->Abort(IndexedDBDatabaseError(
        blink::kWebIDBDatabaseExceptionUnknownError, error_string));
    return;
  }

  AddIndex(object_store_id, std::move(index_metadata), index_id);
  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::CreateIndexAbortOperation, AsWeakPtr(),
                     object_store_id, index_id));
}

void IndexedDBDatabase::CreateIndexAbortOperation(int64_t object_store_id,
                                                  int64_t index_id) {
  IDB_TRACE("IndexedDBDatabase::CreateIndexAbortOperation");
  RemoveIndex(object_store_id, index_id);
}

void IndexedDBDatabase::DeleteIndex(IndexedDBTransaction* transaction,
                                    int64_t object_store_id,
                                    int64_t index_id) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::DeleteIndex", "txn.id", transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!ValidateObjectStoreIdAndIndexId(object_store_id, index_id))
    return;

  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::DeleteIndexOperation, AsWeakPtr(),
                        object_store_id, index_id));
}

Status IndexedDBDatabase::DeleteIndexOperation(
    int64_t object_store_id,
    int64_t index_id,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::DeleteIndexOperation", "txn.id",
             transaction->id());

  IndexedDBIndexMetadata index_metadata =
      RemoveIndex(object_store_id, index_id);

  Status s = metadata_coding_->DeleteIndex(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), object_store_id, index_metadata);

  if (!s.ok())
    return s;

  s = backing_store_->ClearIndex(transaction->BackingStoreTransaction(),
                                 transaction->database()->id(), object_store_id,
                                 index_id);
  if (!s.ok()) {
    AddIndex(object_store_id, std::move(index_metadata),
             IndexedDBIndexMetadata::kInvalidId);
    return s;
  }

  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::DeleteIndexAbortOperation, AsWeakPtr(),
                     object_store_id, std::move(index_metadata)));
  return s;
}

void IndexedDBDatabase::DeleteIndexAbortOperation(
    int64_t object_store_id,
    IndexedDBIndexMetadata index_metadata) {
  IDB_TRACE("IndexedDBDatabase::DeleteIndexAbortOperation");
  AddIndex(object_store_id, std::move(index_metadata),
           IndexedDBIndexMetadata::kInvalidId);
}

void IndexedDBDatabase::RenameIndex(IndexedDBTransaction* transaction,
                                    int64_t object_store_id,
                                    int64_t index_id,
                                    const base::string16& new_name) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::RenameIndex", "txn.id", transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  if (!ValidateObjectStoreIdAndIndexId(object_store_id, index_id))
    return;

  // TODO(dmurph): Remove this call once this method is asynchronous (scheduled
  // on the transaction).
  transaction->EnsureBackingStoreTransactionBegun();

  // Index renaming is done synchronously since preemptive
  // OpenCursor/SetIndexKeys may follow.
  // TODO(dmurph): Make this asynchronous.
  IndexedDBIndexMetadata& index_metadata =
      metadata_.object_stores[object_store_id].indexes[index_id];

  base::string16 old_name;
  Status s = metadata_coding_->RenameIndex(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), object_store_id, new_name, &old_name,
      &index_metadata);

  if (!s.ok()) {
    error_callback_.Run(s, "Internal error renaming index.");
    return;
  }
  DCHECK_EQ(index_metadata.name, new_name);

  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::RenameIndexAbortOperation, AsWeakPtr(),
                     object_store_id, index_id, std::move(old_name)));
}

void IndexedDBDatabase::RenameIndexAbortOperation(int64_t object_store_id,
                                                  int64_t index_id,
                                                  base::string16 old_name) {
  IDB_TRACE("IndexedDBDatabase::RenameIndexAbortOperation");

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[object_store_id];

  DCHECK(object_store.indexes.find(index_id) != object_store.indexes.end());
  object_store.indexes[index_id].name = std::move(old_name);
}

void IndexedDBDatabase::Commit(IndexedDBTransaction* transaction) {
  // The frontend suggests that we commit, but we may have previously initiated
  // an abort, and so have disposed of the transaction. on_abort has already
  // been dispatched to the frontend, so it will find out about that
  // asynchronously.
  if (transaction) {
    // The database can be destroyed in the call to Commit(), so save a copy of
    // the error callback in case of an error.
    auto error_callback_copy = error_callback_;
    Status result = transaction->Commit();
    if (!result.ok())
      error_callback_copy.Run(result, nullptr);
  }
}

void IndexedDBDatabase::AddPendingObserver(
    IndexedDBTransaction* transaction,
    int32_t observer_id,
    const IndexedDBObserver::Options& options) {
  DCHECK(transaction);
  transaction->AddPendingObserver(observer_id, options);
}

void IndexedDBDatabase::CallUpgradeTransactionStartedForTesting(
    int64_t old_version) {
  DCHECK(active_request_);
  active_request_->UpgradeTransactionStarted(old_version);
}

void IndexedDBDatabase::FilterObservation(IndexedDBTransaction* transaction,
                                          int64_t object_store_id,
                                          blink::mojom::IDBOperationType type,
                                          const IndexedDBKeyRange& key_range,
                                          const IndexedDBValue* value) {
  for (auto* connection : connections_) {
    bool recorded = false;
    for (const auto& observer : connection->active_observers()) {
      if (!observer->IsRecordingType(type) ||
          !observer->IsRecordingObjectStore(object_store_id))
        continue;
      if (!recorded) {
        auto observation = blink::mojom::IDBObservation::New();
        observation->object_store_id = object_store_id;
        observation->type = type;
        if (type != blink::mojom::IDBOperationType::Clear)
          observation->key_range = key_range;
        transaction->AddObservation(connection->id(), std::move(observation));
        recorded = true;
      }
      blink::mojom::IDBObserverChangesPtr& changes =
          *transaction->GetPendingChangesForConnection(connection->id());

      changes->observation_index_map[observer->id()].push_back(
          changes->observations.size() - 1);
      if (value && observer->values() && !changes->observations.back()->value) {
        // TODO(dmurph): Avoid any and all IndexedDBValue copies. Perhaps defer
        // this until the end of the transaction, where we can safely erase the
        // indexeddb value. crbug.com/682363
        IndexedDBValue copy = *value;
        changes->observations.back()->value =
            IndexedDBValue::ConvertAndEraseValue(&copy);
      }
    }
  }
}

void IndexedDBDatabase::SendObservations(
    std::map<int32_t, blink::mojom::IDBObserverChangesPtr> changes_map) {
  for (auto* conn : connections_) {
    auto it = changes_map.find(conn->id());
    if (it != changes_map.end())
      conn->callbacks()->OnDatabaseChange(std::move(it->second));
  }
}

void IndexedDBDatabase::GetAll(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    IndexedDBTransaction* transaction,
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    bool key_only,
    int64_t max_count,
    blink::mojom::IDBDatabase::GetAllCallback callback) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::GetAll", "txn.id", transaction->id());

  if (!ValidateObjectStoreId(object_store_id)) {
    IndexedDBDatabaseError error =
        CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                    "Unknown error", transaction);
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBDatabase::GetAllCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBDatabase::GetAllCallback,
                                    blink::mojom::IDBDatabaseGetAllResultPtr>(
          std::move(callback), transaction->AsWeakPtr());

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::GetAllOperation, AsWeakPtr(),
      std::move(dispatcher_host), object_store_id, index_id,
      std::move(key_range),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      max_count, std::move(aborting_callback)));
}

void IndexedDBDatabase::Get(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    IndexedDBTransaction* transaction,
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    bool key_only,
    blink::mojom::IDBDatabase::GetCallback callback) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::Get", "txn.id", transaction->id());

  if (!ValidateObjectStoreIdAndOptionalIndexId(object_store_id, index_id)) {
    IndexedDBDatabaseError error =
        CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                    "Unknown error", transaction);
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  blink::mojom::IDBDatabase::GetCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBDatabase::GetCallback,
                                    blink::mojom::IDBDatabaseGetResultPtr>(
          std::move(callback), transaction->AsWeakPtr());

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::GetOperation, AsWeakPtr(), std::move(dispatcher_host),
      object_store_id, index_id, std::move(key_range),
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE,
      std::move(aborting_callback)));
}

Status IndexedDBDatabase::GetOperation(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    indexed_db::CursorType cursor_type,
    blink::mojom::IDBDatabase::GetCallback callback,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::GetOperation", "txn.id", transaction->id());

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  const IndexedDBKey* key;

  Status s = Status::OK();
  if (!dispatcher_host) {
    IndexedDBDatabaseError error =
        CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                    "Unknown error", transaction);
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  std::unique_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;
  if (key_range->IsOnlyKey()) {
    key = &key_range->lower();
  } else {
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // ObjectStore Retrieval Operation
      if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
        backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
            transaction->BackingStoreTransaction(), id(), object_store_id,
            *key_range, blink::mojom::IDBCursorDirection::Next, &s);
      } else {
        backing_store_cursor = backing_store_->OpenObjectStoreCursor(
            transaction->BackingStoreTransaction(), id(), object_store_id,
            *key_range, blink::mojom::IDBCursorDirection::Next, &s);
      }
    } else if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      // Index Value Retrieval Operation
      backing_store_cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    } else {
      // Index Referenced Value Retrieval Operation
      backing_store_cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    }

    if (!s.ok()) {
      IndexedDBDatabaseError error =
          CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                      "Corruption detected, unable to continue", transaction);
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      return s;
    }

    if (!backing_store_cursor) {
      // This means we've run out of data.
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
      return s;
    }

    key = &backing_store_cursor->key();
  }

  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    // Object Store Retrieval Operation
    IndexedDBReturnValue value;
    s = backing_store_->GetRecord(transaction->BackingStoreTransaction(), id(),
                                  object_store_id, *key, &value);
    if (!s.ok()) {
      IndexedDBDatabaseError error =
          CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                      "Unknown error", transaction);
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      return s;
    }

    if (value.empty()) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
      return s;
    }

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewKey(std::move(*key)));
      return s;
    }

    if (object_store_metadata.auto_increment &&
        !object_store_metadata.key_path.IsNull()) {
      value.primary_key = *key;
      value.key_path = object_store_metadata.key_path;
    }

    blink::mojom::IDBReturnValuePtr mojo_value =
        IndexedDBReturnValue::ConvertReturnValue(&value);

    std::vector<IndexedDBCallbacks::IndexedDBValueBlob> value_blob;
    IndexedDBCallbacks::IndexedDBValueBlob::GetIndexedDBValueBlobs(
        &value_blob, value.blob_info, &mojo_value->value->blob_or_file_info);

    if (!IndexedDBCallbacks::CreateAllBlobs(
            dispatcher_host->blob_storage_context(), std::move(value_blob))) {
      IndexedDBDatabaseError error =
          CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                      "Unknown error", transaction);
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      return s;
    }

    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetResult::NewValue(std::move(mojo_value)));
    return s;
  }

  // From here we are dealing only with indexes.
  std::unique_ptr<IndexedDBKey> primary_key;
  s = backing_store_->GetPrimaryKeyViaIndex(
      transaction->BackingStoreTransaction(), id(), object_store_id, index_id,
      *key, &primary_key);
  if (!s.ok()) {
    IndexedDBDatabaseError error =
        CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                    "Unknown error", transaction);
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  if (!primary_key) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
    return s;
  }
  if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
    // Index Value Retrieval Operation
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetResult::NewKey(std::move(*primary_key)));
    return s;
  }

  // Index Referenced Value Retrieval Operation
  IndexedDBReturnValue value;
  s = backing_store_->GetRecord(transaction->BackingStoreTransaction(), id(),
                                object_store_id, *primary_key, &value);
  if (!s.ok()) {
    IndexedDBDatabaseError error =
        CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                    "Unknown error", transaction);
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  if (value.empty()) {
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewEmpty(true));
    return s;
  }
  if (object_store_metadata.auto_increment &&
      !object_store_metadata.key_path.IsNull()) {
    value.primary_key = *primary_key;
    value.key_path = object_store_metadata.key_path;
  }

  blink::mojom::IDBReturnValuePtr mojo_value =
      IndexedDBReturnValue::ConvertReturnValue(&value);

  std::vector<IndexedDBCallbacks::IndexedDBValueBlob> value_blob;
  IndexedDBCallbacks::IndexedDBValueBlob::GetIndexedDBValueBlobs(
      &value_blob, value.blob_info, &mojo_value->value->blob_or_file_info);

  if (!IndexedDBCallbacks::CreateAllBlobs(
          dispatcher_host->blob_storage_context(), std::move(value_blob))) {
    IndexedDBDatabaseError error =
        CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                    "Unknown error", transaction);
    std::move(callback).Run(blink::mojom::IDBDatabaseGetResult::NewErrorResult(
        blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  std::move(callback).Run(
      blink::mojom::IDBDatabaseGetResult::NewValue(std::move(mojo_value)));
  return s;
}

static_assert(sizeof(size_t) >= sizeof(int32_t),
              "Size of size_t is less than size of int32");
static_assert(blink::mojom::kIDBMaxMessageOverhead <= INT32_MAX,
              "kIDBMaxMessageOverhead is more than INT32_MAX");

Status IndexedDBDatabase::GetAllOperation(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    indexed_db::CursorType cursor_type,
    int64_t max_count,
    blink::mojom::IDBDatabase::GetAllCallback callback,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::GetAllOperation", "txn.id", transaction->id());

  DCHECK_GT(max_count, 0);

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];

  Status s = Status::OK();
  if (!dispatcher_host) {
    IndexedDBDatabaseError error =
        CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                    "Unknown error", transaction);
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  std::unique_ptr<IndexedDBBackingStore::Cursor> cursor;

  if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
    // Retrieving keys
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Key Retrieval Operation
      cursor = backing_store_->OpenObjectStoreKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    } else {
      // Index Value: (Primary Key) Retrieval Operation
      cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    }
  } else {
    // Retrieving values
    if (index_id == IndexedDBIndexMetadata::kInvalidId) {
      // Object Store: Value Retrieval Operation
      cursor = backing_store_->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    } else {
      // Object Store: Referenced Value Retrieval Operation
      cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), object_store_id,
          index_id, *key_range, blink::mojom::IDBCursorDirection::Next, &s);
    }
  }

  if (!s.ok()) {
    DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
    IndexedDBDatabaseError error =
        CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                    "Corruption detected, unable to continue", transaction);
    std::move(callback).Run(
        blink::mojom::IDBDatabaseGetAllResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return s;
  }

  std::vector<IndexedDBKey> found_keys;
  std::vector<IndexedDBReturnValue> found_values;
  if (!cursor) {
    // Doesn't matter if key or value array here - will be empty array when it
    // hits JavaScript.
    std::vector<blink::mojom::IDBReturnValuePtr> mojo_found_values;
    std::move(callback).Run(blink::mojom::IDBDatabaseGetAllResult::NewValues(
        std::move(mojo_found_values)));
    return s;
  }

  bool did_first_seek = false;
  bool generated_key = object_store_metadata.auto_increment &&
                       !object_store_metadata.key_path.IsNull();

  size_t response_size = blink::mojom::kIDBMaxMessageOverhead;
  int64_t num_found_items = 0;
  while (num_found_items++ < max_count) {
    bool cursor_valid;
    if (did_first_seek) {
      cursor_valid = cursor->Continue(&s);
    } else {
      cursor_valid = cursor->FirstSeek(&s);
      did_first_seek = true;
    }
    if (!s.ok()) {
      IndexedDBDatabaseError error =
          CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                      "Seek failure, unable to continue", transaction);
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetAllResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      return s;
    }

    if (!cursor_valid)
      break;

    IndexedDBReturnValue return_value;
    IndexedDBKey return_key;

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      return_key = cursor->primary_key();
    } else {
      // Retrieving values
      return_value.swap(*cursor->value());
      if (!return_value.empty() && generated_key) {
        return_value.primary_key = cursor->primary_key();
        return_value.key_path = object_store_metadata.key_path;
      }
    }

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY)
      response_size += return_key.size_estimate();
    else
      response_size += return_value.SizeEstimate();
    if (response_size > GetUsableMessageSizeInBytes()) {
      IndexedDBDatabaseError error =
          CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                      "Maximum IPC message size exceeded.", transaction);
      std::move(callback).Run(
          blink::mojom::IDBDatabaseGetAllResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      return s;
    }

    if (cursor_type == indexed_db::CURSOR_KEY_ONLY)
      found_keys.push_back(return_key);
    else
      found_values.push_back(return_value);
  }

  if (cursor_type == indexed_db::CURSOR_KEY_ONLY) {
    // IndexedDBKey already supports an array of values so we can leverage  this
    // to return an array of keys - no need to create our own array of keys.
    std::move(callback).Run(blink::mojom::IDBDatabaseGetAllResult::NewKey(
        IndexedDBKey(std::move(found_keys))));
    return s;
  }

  std::vector<blink::mojom::IDBReturnValuePtr> mojo_values;
  mojo_values.reserve(found_values.size());
  for (size_t i = 0; i < found_values.size(); ++i) {
    mojo_values.push_back(
        IndexedDBReturnValue::ConvertReturnValue(&found_values[i]));
  }

  std::vector<IndexedDBCallbacks::IndexedDBValueBlob> value_blobs;
  for (size_t i = 0; i < mojo_values.size(); ++i) {
    IndexedDBCallbacks::IndexedDBValueBlob::GetIndexedDBValueBlobs(
        &value_blobs, found_values[i].blob_info,
        &mojo_values[i]->value->blob_or_file_info);
  }

  if (!IndexedDBCallbacks::CreateAllBlobs(
          dispatcher_host->blob_storage_context(), std::move(value_blobs))) {
    return s;
  }

  std::move(callback).Run(
      blink::mojom::IDBDatabaseGetAllResult::NewValues(std::move(mojo_values)));
  return s;
}

static std::unique_ptr<IndexedDBKey> GenerateKey(
    IndexedDBBackingStore* backing_store,
    IndexedDBTransaction* transaction,
    int64_t database_id,
    int64_t object_store_id) {
  // Maximum integer uniquely representable as ECMAScript number.
  const int64_t max_generator_value = 9007199254740992LL;
  int64_t current_number;
  Status s = backing_store->GetKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(), database_id, object_store_id,
      &current_number);
  if (!s.ok()) {
    LOG(ERROR) << "Failed to GetKeyGeneratorCurrentNumber";
    return std::make_unique<IndexedDBKey>();
  }
  if (current_number < 0 || current_number > max_generator_value)
    return std::make_unique<IndexedDBKey>();

  return std::make_unique<IndexedDBKey>(current_number,
                                        blink::mojom::IDBKeyType::Number);
}

// Called at the end of a "put" operation. The key is a number that was either
// generated by the generator which now needs to be incremented (so
// |check_current| is false) or was user-supplied so we only conditionally use
// (and |check_current| is true).
static Status UpdateKeyGenerator(IndexedDBBackingStore* backing_store,
                                 IndexedDBTransaction* transaction,
                                 int64_t database_id,
                                 int64_t object_store_id,
                                 const IndexedDBKey& key,
                                 bool check_current) {
  DCHECK_EQ(blink::mojom::IDBKeyType::Number, key.type());
  // Maximum integer uniquely representable as ECMAScript number.
  const double max_generator_value = 9007199254740992.0;
  int64_t value = base::saturated_cast<int64_t>(
      floor(std::min(key.number(), max_generator_value)));
  return backing_store->MaybeUpdateKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(), database_id, object_store_id,
      value + 1, check_current);
}

struct IndexedDBDatabase::PutOperationParams {
  PutOperationParams() {}
  int64_t object_store_id;
  IndexedDBValue value;
  std::unique_ptr<IndexedDBKey> key;
  blink::mojom::IDBPutMode put_mode;
  scoped_refptr<IndexedDBCallbacks> callbacks;
  std::vector<IndexedDBIndexKeys> index_keys;

 private:
  DISALLOW_COPY_AND_ASSIGN(PutOperationParams);
};

void IndexedDBDatabase::Put(IndexedDBTransaction* transaction,
                            int64_t object_store_id,
                            IndexedDBValue* value,
                            std::unique_ptr<IndexedDBKey> key,
                            blink::mojom::IDBPutMode put_mode,
                            scoped_refptr<IndexedDBCallbacks> callbacks,
                            const std::vector<IndexedDBIndexKeys>& index_keys) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::Put", "txn.id", transaction->id());
  DCHECK_NE(transaction->mode(), blink::mojom::IDBTransactionMode::ReadOnly);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  DCHECK(key);
  DCHECK(value);
  std::unique_ptr<PutOperationParams> params(
      std::make_unique<PutOperationParams>());
  params->object_store_id = object_store_id;
  params->value.swap(*value);
  params->key = std::move(key);
  params->put_mode = put_mode;
  params->callbacks = callbacks;
  params->index_keys = index_keys;
  transaction->ScheduleTask(BindWeakOperation(&IndexedDBDatabase::PutOperation,
                                              AsWeakPtr(), std::move(params)));
}

Status IndexedDBDatabase::PutOperation(
    std::unique_ptr<PutOperationParams> params,
    IndexedDBTransaction* transaction) {
  IDB_TRACE2("IndexedDBDatabase::PutOperation", "txn.id", transaction->id(),
             "size", params->value.SizeEstimate());
  DCHECK_NE(transaction->mode(), blink::mojom::IDBTransactionMode::ReadOnly);
  bool key_was_generated = false;
  Status s = Status::OK();

  DCHECK(metadata_.object_stores.find(params->object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store =
      metadata_.object_stores[params->object_store_id];
  DCHECK(object_store.auto_increment || params->key->IsValid());

  std::unique_ptr<IndexedDBKey> key;
  if (params->put_mode != blink::mojom::IDBPutMode::CursorUpdate &&
      object_store.auto_increment && !params->key->IsValid()) {
    std::unique_ptr<IndexedDBKey> auto_inc_key =
        GenerateKey(backing_store_, transaction, id(), params->object_store_id);
    key_was_generated = true;
    if (!auto_inc_key->IsValid()) {
      params->callbacks->OnError(
          CreateError(blink::kWebIDBDatabaseExceptionConstraintError,
                      "Maximum key generator value reached.", transaction));
      return s;
    }
    key = std::move(auto_inc_key);
  } else {
    key = std::move(params->key);
  }

  DCHECK(key->IsValid());

  IndexedDBBackingStore::RecordIdentifier record_identifier;
  if (params->put_mode == blink::mojom::IDBPutMode::AddOnly) {
    bool found = false;
    Status found_status = backing_store_->KeyExistsInObjectStore(
        transaction->BackingStoreTransaction(), id(), params->object_store_id,
        *key, &record_identifier, &found);
    if (!found_status.ok())
      return found_status;
    if (found) {
      params->callbacks->OnError(
          CreateError(blink::kWebIDBDatabaseExceptionConstraintError,
                      "Key already exists in the object store.", transaction));
      return found_status;
    }
  }

  std::vector<std::unique_ptr<IndexWriter>> index_writers;
  base::string16 error_message;
  bool obeys_constraints = false;
  bool backing_store_success = MakeIndexWriters(
      transaction, backing_store_, id(), object_store, *key, key_was_generated,
      params->index_keys, &index_writers, &error_message, &obeys_constraints);
  if (!backing_store_success) {
    params->callbacks->OnError(
        CreateError(blink::kWebIDBDatabaseExceptionUnknownError,
                    "Internal error: backing store error updating index keys.",
                    transaction));
    return s;
  }
  if (!obeys_constraints) {
    params->callbacks->OnError(
        CreateError(blink::kWebIDBDatabaseExceptionConstraintError,
                    error_message, transaction));
    return s;
  }

  // Before this point, don't do any mutation. After this point, rollback the
  // transaction in case of error.
  s = backing_store_->PutRecord(transaction->BackingStoreTransaction(), id(),
                                params->object_store_id, *key, &params->value,
                                &record_identifier);
  if (!s.ok())
    return s;

  {
    IDB_TRACE1("IndexedDBDatabase::PutOperation.UpdateIndexes", "txn.id",
               transaction->id());
    for (const auto& writer : index_writers) {
      writer->WriteIndexKeys(record_identifier, backing_store_,
                             transaction->BackingStoreTransaction(), id(),
                             params->object_store_id);
    }
  }

  if (object_store.auto_increment &&
      params->put_mode != blink::mojom::IDBPutMode::CursorUpdate &&
      key->type() == blink::mojom::IDBKeyType::Number) {
    IDB_TRACE1("IndexedDBDatabase::PutOperation.AutoIncrement", "txn.id",
               transaction->id());
    s = UpdateKeyGenerator(backing_store_, transaction, id(),
                           params->object_store_id, *key, !key_was_generated);
    if (!s.ok())
      return s;
  }
  {
    IDB_TRACE1("IndexedDBDatabase::PutOperation.Callbacks", "txn.id",
               transaction->id());
    params->callbacks->OnSuccess(*key);
  }
  FilterObservation(transaction, params->object_store_id,
                    params->put_mode == blink::mojom::IDBPutMode::AddOnly
                        ? blink::mojom::IDBOperationType::Add
                        : blink::mojom::IDBOperationType::Put,
                    IndexedDBKeyRange(*key), &params->value);
  factory_->NotifyIndexedDBContentChanged(
      origin(), metadata_.name,
      metadata_.object_stores[params->object_store_id].name);
  return s;
}

void IndexedDBDatabase::SetIndexKeys(
    IndexedDBTransaction* transaction,
    int64_t object_store_id,
    std::unique_ptr<IndexedDBKey> primary_key,
    const std::vector<IndexedDBIndexKeys>& index_keys) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::SetIndexKeys", "txn.id", transaction->id());
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  // TODO(dmurph): Remove this call once this method is asynchronous (scheduled
  // on the transaction).
  transaction->EnsureBackingStoreTransactionBegun();

  // TODO(dmurph): This methods should be turned asynchronous, and not rely on
  // the renderer to have matching calls to OpenCursor and SetIndexedsReady.
  IndexedDBBackingStore::RecordIdentifier record_identifier;
  bool found = false;
  Status s = backing_store_->KeyExistsInObjectStore(
      transaction->BackingStoreTransaction(), metadata_.id, object_store_id,
      *primary_key, &record_identifier, &found);
  if (!s.ok()) {
    error_callback_.Run(s, "Internal error setting index keys.");
    return;
  }
  if (!found) {
    transaction->Abort(IndexedDBDatabaseError(
        blink::kWebIDBDatabaseExceptionUnknownError,
        "Internal error setting index keys for object store."));
    return;
  }

  std::vector<std::unique_ptr<IndexWriter>> index_writers;
  base::string16 error_message;
  bool obeys_constraints = false;
  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  const IndexedDBObjectStoreMetadata& object_store_metadata =
      metadata_.object_stores[object_store_id];
  bool backing_store_success = MakeIndexWriters(
      transaction, backing_store_, id(), object_store_metadata, *primary_key,
      false, index_keys, &index_writers, &error_message, &obeys_constraints);
  if (!backing_store_success) {
    transaction->Abort(IndexedDBDatabaseError(
        blink::kWebIDBDatabaseExceptionUnknownError,
        "Internal error: backing store error updating index keys."));
    return;
  }
  if (!obeys_constraints) {
    transaction->Abort(IndexedDBDatabaseError(
        blink::kWebIDBDatabaseExceptionConstraintError, error_message));
    return;
  }

  for (const auto& writer : index_writers) {
    writer->WriteIndexKeys(record_identifier, backing_store_,
                           transaction->BackingStoreTransaction(), id(),
                           object_store_id);
  }
}

void IndexedDBDatabase::SetIndexesReady(IndexedDBTransaction* transaction,
                                        int64_t,
                                        const std::vector<int64_t>& index_ids) {
  DCHECK(transaction);
  DCHECK_EQ(transaction->mode(),
            blink::mojom::IDBTransactionMode::VersionChange);

  transaction->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(&IndexedDBDatabase::SetIndexesReadyOperation,
                        AsWeakPtr(), index_ids.size()));
}

Status IndexedDBDatabase::SetIndexesReadyOperation(
    size_t index_count,
    IndexedDBTransaction* transaction) {
  // TODO(dmurph): This method should be refactored out for something more
  // reliable.
  for (size_t i = 0; i < index_count; ++i)
    transaction->DidCompletePreemptiveEvent();
  return Status::OK();
}

struct IndexedDBDatabase::OpenCursorOperationParams {
  OpenCursorOperationParams() {}
  int64_t object_store_id;
  int64_t index_id;
  std::unique_ptr<IndexedDBKeyRange> key_range;
  blink::mojom::IDBCursorDirection direction;
  indexed_db::CursorType cursor_type;
  blink::mojom::IDBTaskType task_type;
  scoped_refptr<IndexedDBCallbacks> callbacks;

 private:
  DISALLOW_COPY_AND_ASSIGN(OpenCursorOperationParams);
};

void IndexedDBDatabase::OpenCursor(
    IndexedDBTransaction* transaction,
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    blink::mojom::IDBCursorDirection direction,
    bool key_only,
    blink::mojom::IDBTaskType task_type,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::OpenCursor", "txn.id", transaction->id());

  if (!ValidateObjectStoreIdAndOptionalIndexId(object_store_id, index_id))
    return;

  std::unique_ptr<OpenCursorOperationParams> params(
      std::make_unique<OpenCursorOperationParams>());
  params->object_store_id = object_store_id;
  params->index_id = index_id;
  params->key_range = std::move(key_range);
  params->direction = direction;
  params->cursor_type =
      key_only ? indexed_db::CURSOR_KEY_ONLY : indexed_db::CURSOR_KEY_AND_VALUE;
  params->task_type = task_type;
  params->callbacks = callbacks;
  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::OpenCursorOperation, AsWeakPtr(), std::move(params)));
}

Status IndexedDBDatabase::OpenCursorOperation(
    std::unique_ptr<OpenCursorOperationParams> params,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::OpenCursorOperation", "txn.id",
             transaction->id());

  // The frontend has begun indexing, so this pauses the transaction
  // until the indexing is complete. This can't happen any earlier
  // because we don't want to switch to early mode in case multiple
  // indexes are being created in a row, with Put()'s in between.
  if (params->task_type == blink::mojom::IDBTaskType::Preemptive)
    transaction->AddPreemptiveEvent();

  Status s = Status::OK();
  std::unique_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;
  if (params->index_id == IndexedDBIndexMetadata::kInvalidId) {
    if (params->cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      DCHECK_EQ(params->task_type, blink::mojom::IDBTaskType::Normal);
      backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          *params->key_range, params->direction, &s);
    } else {
      backing_store_cursor = backing_store_->OpenObjectStoreCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          *params->key_range, params->direction, &s);
    }
  } else {
    DCHECK_EQ(params->task_type, blink::mojom::IDBTaskType::Normal);
    if (params->cursor_type == indexed_db::CURSOR_KEY_ONLY) {
      backing_store_cursor = backing_store_->OpenIndexKeyCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          params->index_id, *params->key_range, params->direction, &s);
    } else {
      backing_store_cursor = backing_store_->OpenIndexCursor(
          transaction->BackingStoreTransaction(), id(), params->object_store_id,
          params->index_id, *params->key_range, params->direction, &s);
    }
  }

  if (!s.ok()) {
    DLOG(ERROR) << "Unable to open cursor operation: " << s.ToString();
    return s;
  }

  if (!backing_store_cursor) {
    // Occurs when we've reached the end of cursor's data.
    params->callbacks->OnSuccess(nullptr);
    return s;
  }

  std::unique_ptr<IndexedDBCursor> cursor = std::make_unique<IndexedDBCursor>(
      std::move(backing_store_cursor), params->cursor_type, params->task_type,
      transaction->AsWeakPtr());
  IndexedDBCursor* cursor_ptr = cursor.get();
  transaction->RegisterOpenCursor(cursor_ptr);
  params->callbacks->OnSuccess(std::move(cursor), cursor_ptr->key(),
                               cursor_ptr->primary_key(), cursor_ptr->Value());
  return s;
}

void IndexedDBDatabase::Count(IndexedDBTransaction* transaction,
                              int64_t object_store_id,
                              int64_t index_id,
                              std::unique_ptr<IndexedDBKeyRange> key_range,
                              scoped_refptr<IndexedDBCallbacks> callbacks) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::Count", "txn.id", transaction->id());

  if (!ValidateObjectStoreIdAndOptionalIndexId(object_store_id, index_id))
    return;

  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::CountOperation, AsWeakPtr(), object_store_id,
      index_id, std::move(key_range), callbacks));
}

Status IndexedDBDatabase::CountOperation(
    int64_t object_store_id,
    int64_t index_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::CountOperation", "txn.id", transaction->id());
  uint32_t count = 0;
  std::unique_ptr<IndexedDBBackingStore::Cursor> backing_store_cursor;

  Status s = Status::OK();
  if (index_id == IndexedDBIndexMetadata::kInvalidId) {
    backing_store_cursor = backing_store_->OpenObjectStoreKeyCursor(
        transaction->BackingStoreTransaction(), id(), object_store_id,
        *key_range, blink::mojom::IDBCursorDirection::Next, &s);
  } else {
    backing_store_cursor = backing_store_->OpenIndexKeyCursor(
        transaction->BackingStoreTransaction(), id(), object_store_id, index_id,
        *key_range, blink::mojom::IDBCursorDirection::Next, &s);
  }
  if (!s.ok()) {
    DLOG(ERROR) << "Unable perform count operation: " << s.ToString();
    return s;
  }
  if (!backing_store_cursor) {
    callbacks->OnSuccess(count);
    return s;
  }

  do {
    if (!s.ok())
      return s;
    ++count;
  } while (backing_store_cursor->Continue(&s));

  callbacks->OnSuccess(count);
  return s;
}

void IndexedDBDatabase::DeleteRange(
    IndexedDBTransaction* transaction,
    int64_t object_store_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::DeleteRange", "txn.id", transaction->id());
  DCHECK_NE(transaction->mode(), blink::mojom::IDBTransactionMode::ReadOnly);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::DeleteRangeOperation, AsWeakPtr(),
                        object_store_id, std::move(key_range), callbacks));
}

Status IndexedDBDatabase::DeleteRangeOperation(
    int64_t object_store_id,
    std::unique_ptr<IndexedDBKeyRange> key_range,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::DeleteRangeOperation", "txn.id",
             transaction->id());
  Status s = backing_store_->DeleteRange(transaction->BackingStoreTransaction(),
                                         id(), object_store_id, *key_range);
  if (!s.ok())
    return s;
  callbacks->OnSuccess();
  FilterObservation(transaction, object_store_id,
                    blink::mojom::IDBOperationType::Delete, *key_range,
                    nullptr);
  factory_->NotifyIndexedDBContentChanged(
      origin(), metadata_.name, metadata_.object_stores[object_store_id].name);
  return s;
}

void IndexedDBDatabase::GetKeyGeneratorCurrentNumber(
    IndexedDBTransaction* transaction,
    int64_t object_store_id,
    scoped_refptr<IndexedDBCallbacks> callbacks) {
  DCHECK(transaction);
  if (!ValidateObjectStoreId(object_store_id)) {
    callbacks->OnError(CreateError(blink::kWebIDBDatabaseExceptionDataError,
                                   "Object store id not valid.", transaction));
    return;
  }
  transaction->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::GetKeyGeneratorCurrentNumberOperation, AsWeakPtr(),
      object_store_id, callbacks));
}

Status IndexedDBDatabase::GetKeyGeneratorCurrentNumberOperation(
    int64_t object_store_id,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  int64_t current_number;
  Status s = backing_store_->GetKeyGeneratorCurrentNumber(
      transaction->BackingStoreTransaction(), id(), object_store_id,
      &current_number);
  if (!s.ok()) {
    callbacks->OnError(CreateError(
        blink::kWebIDBDatabaseExceptionDataError,
        "Failed to get the current number of key generator.", transaction));
    return s;
  }
  callbacks->OnSuccess(current_number);
  return s;
}

void IndexedDBDatabase::Clear(IndexedDBTransaction* transaction,
                              int64_t object_store_id,
                              scoped_refptr<IndexedDBCallbacks> callbacks) {
  DCHECK(transaction);
  IDB_TRACE1("IndexedDBDatabase::Clear", "txn.id", transaction->id());
  DCHECK_NE(transaction->mode(), blink::mojom::IDBTransactionMode::ReadOnly);

  if (!ValidateObjectStoreId(object_store_id))
    return;

  transaction->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::ClearOperation, AsWeakPtr(),
                        object_store_id, callbacks));
}

Status IndexedDBDatabase::ClearOperation(
    int64_t object_store_id,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::ClearOperation", "txn.id", transaction->id());
  Status s = backing_store_->ClearObjectStore(
      transaction->BackingStoreTransaction(), id(), object_store_id);
  if (!s.ok())
    return s;
  callbacks->OnSuccess();

  FilterObservation(transaction, object_store_id,
                    blink::mojom::IDBOperationType::Clear, IndexedDBKeyRange(),
                    nullptr);
  factory_->NotifyIndexedDBContentChanged(
      origin(), metadata_.name, metadata_.object_stores[object_store_id].name);
  return s;
}

Status IndexedDBDatabase::DeleteObjectStoreOperation(
    int64_t object_store_id,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::DeleteObjectStoreOperation", "txn.id",
             transaction->id());

  IndexedDBObjectStoreMetadata object_store_metadata =
      RemoveObjectStore(object_store_id);

  // First remove metadata.
  Status s = metadata_coding_->DeleteObjectStore(
      transaction->BackingStoreTransaction()->transaction(),
      transaction->database()->id(), object_store_metadata);

  if (!s.ok()) {
    AddObjectStore(std::move(object_store_metadata),
                   IndexedDBObjectStoreMetadata::kInvalidId);
    return s;
  }

  // Then remove object store contents.
  s = backing_store_->ClearObjectStore(transaction->BackingStoreTransaction(),
                                       transaction->database()->id(),
                                       object_store_id);

  if (!s.ok()) {
    AddObjectStore(std::move(object_store_metadata),
                   IndexedDBObjectStoreMetadata::kInvalidId);
    return s;
  }
  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::DeleteObjectStoreAbortOperation,
                     AsWeakPtr(), std::move(object_store_metadata)));
  return s;
}

Status IndexedDBDatabase::VersionChangeOperation(
    int64_t version,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::VersionChangeOperation", "txn.id",
             transaction->id());
  int64_t old_version = metadata_.version;
  DCHECK_GT(version, old_version);

  metadata_coding_->SetDatabaseVersion(
      transaction->BackingStoreTransaction()->transaction(), id(), version,
      &metadata_);

  transaction->ScheduleAbortTask(
      base::BindOnce(&IndexedDBDatabase::VersionChangeAbortOperation,
                     AsWeakPtr(), old_version));

  active_request_->CreateAndBindTransaction();
  active_request_->UpgradeTransactionStarted(old_version);
  return Status::OK();
}

void IndexedDBDatabase::TransactionCreated() {
  UMA_HISTOGRAM_COUNTS_1000(
      "WebCore.IndexedDB.Database.OutstandingTransactionCount",
      transaction_count_);
  ++transaction_count_;
}

void IndexedDBDatabase::TransactionFinished(
    blink::mojom::IDBTransactionMode mode,
    bool committed) {
  --transaction_count_;
  DCHECK_GE(transaction_count_, 0);

  // TODO(dmurph): To help remove this integration with IndexedDBDatabase, make
  // a 'committed' listener closure on all transactions. Then the request can
  // just listen for that.

  // This may be an unrelated transaction finishing while waiting for
  // connections to close, or the actual upgrade transaction from an active
  // request. Notify the active request if it's the latter.
  if (active_request_ &&
      mode == blink::mojom::IDBTransactionMode::VersionChange &&
      !force_closing_) {
    active_request_->UpgradeTransactionFinished(committed);
  }
}

void IndexedDBDatabase::AppendRequest(
    std::unique_ptr<ConnectionRequest> request) {
  pending_requests_.push(std::move(request));

  if (!active_request_)
    ProcessRequestQueueAndMaybeRelease();
}

void IndexedDBDatabase::RequestComplete(ConnectionRequest* request) {
  DCHECK_EQ(request, active_request_.get());
  // Destroying a request can cause this instance to be destroyed (through
  // ConnectionClosed), so hold a WeakPtr.
  base::WeakPtr<IndexedDBDatabase> weak_ptr = weak_factory_.GetWeakPtr();
  active_request_.reset();

  if (!weak_ptr)
    return;

  ProcessRequestQueueAndMaybeRelease();
}

void IndexedDBDatabase::ProcessRequestQueueAndMaybeRelease() {
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
  MaybeReleaseDatabase();
}

void IndexedDBDatabase::MaybeReleaseDatabase() {
  if (!active_request_ && pending_requests_.empty() && connections_.empty() &&
      !force_closing_)
    std::move(destroy_me_).Run();
}

void IndexedDBDatabase::RegisterAndScheduleTransaction(
    IndexedDBTransaction* transaction) {
  IDB_TRACE1("IndexedDBDatabase::RegisterAndScheduleTransaction", "txn.id",
             transaction->id());
  std::vector<ScopesLockManager::ScopeLockRequest> lock_requests;
  lock_requests.reserve(1 + transaction->scope().size());
  lock_requests.emplace_back(
      kDatabaseRangeLockLevel, GetDatabaseLockRange(id()),
      transaction->mode() == blink::mojom::IDBTransactionMode::VersionChange
          ? ScopesLockManager::LockType::kExclusive
          : ScopesLockManager::LockType::kShared);
  ScopesLockManager::LockType lock_type =
      transaction->mode() == blink::mojom::IDBTransactionMode::ReadOnly
          ? ScopesLockManager::LockType::kShared
          : ScopesLockManager::LockType::kExclusive;
  for (int64_t object_store : transaction->scope()) {
    lock_requests.emplace_back(kObjectStoreRangeLockLevel,
                               GetObjectStoreLockRange(id(), object_store),
                               lock_type);
  }
  lock_manager_->AcquireLocks(
      std::move(lock_requests),
      transaction->locks_receiver()->weak_factory.GetWeakPtr(),
      base::BindOnce(&IndexedDBTransaction::Start, transaction->AsWeakPtr()));
}

void IndexedDBDatabase::ScheduleOpenConnection(
    IndexedDBOriginStateHandle origin_state_handle,
    std::unique_ptr<IndexedDBPendingConnection> connection) {
  AppendRequest(std::make_unique<OpenRequest>(std::move(origin_state_handle),
                                              this, std::move(connection)));
}

void IndexedDBDatabase::ScheduleDeleteDatabase(
    IndexedDBOriginStateHandle origin_state_handle,
    scoped_refptr<IndexedDBCallbacks> callbacks,
    base::OnceClosure on_deletion_complete) {
  AppendRequest(std::make_unique<DeleteRequest>(
      std::move(origin_state_handle), this, callbacks,
      std::move(on_deletion_complete)));
}

void IndexedDBDatabase::ForceClose() {
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
    ProcessRequestQueueAndMaybeRelease();
}
void IndexedDBDatabase::CreateObjectStoreAbortOperation(
    int64_t object_store_id) {
  IDB_TRACE("IndexedDBDatabase::CreateObjectStoreAbortOperation");
  RemoveObjectStore(object_store_id);
}

void IndexedDBDatabase::DeleteObjectStoreAbortOperation(
    IndexedDBObjectStoreMetadata object_store_metadata) {
  IDB_TRACE("IndexedDBDatabase::DeleteObjectStoreAbortOperation");
  AddObjectStore(std::move(object_store_metadata),
                 IndexedDBObjectStoreMetadata::kInvalidId);
}

void IndexedDBDatabase::RenameObjectStoreAbortOperation(
    int64_t object_store_id,
    base::string16 old_name) {
  IDB_TRACE("IndexedDBDatabase::RenameObjectStoreAbortOperation");

  DCHECK(metadata_.object_stores.find(object_store_id) !=
         metadata_.object_stores.end());
  metadata_.object_stores[object_store_id].name = std::move(old_name);
}

void IndexedDBDatabase::VersionChangeAbortOperation(int64_t previous_version) {
  IDB_TRACE("IndexedDBDatabase::VersionChangeAbortOperation");
  metadata_.version = previous_version;
}

void IndexedDBDatabase::AbortAllTransactionsForConnections() {
  IDB_TRACE("IndexedDBDatabase::AbortAllTransactionsForConnections");

  for (IndexedDBConnection* connection : connections_) {
    connection->FinishAllTransactions(
        IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionUnknownError,
                               "Database is compacting."));
  }
}

}  // namespace content
