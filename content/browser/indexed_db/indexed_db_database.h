// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db.h"
#include "content/browser/indexed_db/indexed_db_backing_store.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_observer.h"
#include "content/browser/indexed_db/indexed_db_origin_state_handle.h"
#include "content/browser/indexed_db/indexed_db_pending_connection.h"
#include "content/browser/indexed_db/list_set.h"
#include "content/browser/indexed_db/scopes/scopes_lock_manager.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_key.h"
#include "third_party/blink/public/common/indexeddb/web_idb_types.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace blink {
class IndexedDBKeyPath;
class IndexedDBKeyRange;
struct IndexedDBDatabaseMetadata;
struct IndexedDBIndexMetadata;
struct IndexedDBObjectStoreMetadata;
}  // namespace blink

namespace url {
class Origin;
}

namespace content {
class IndexedDBClassFactory;
class IndexedDBConnection;
class IndexedDBDatabaseCallbacks;
class IndexedDBFactory;
class IndexedDBMetadataCoding;
class IndexedDBOriginStateHandle;
class IndexedDBTransaction;
struct IndexedDBValue;

class CONTENT_EXPORT IndexedDBDatabase {
 public:
  // Identifier is pair of (origin, database name).
  using Identifier = std::pair<url::Origin, base::string16>;
  // Used to report irrecoverable backend errors. The second argument can be
  // null.
  using ErrorCallback =
      base::RepeatingCallback<void(leveldb::Status, const char*)>;

  static const int64_t kInvalidId = 0;
  static const int64_t kMinimumIndexId = 30;

  virtual ~IndexedDBDatabase();

  const Identifier& identifier() const { return identifier_; }
  IndexedDBBackingStore* backing_store() { return backing_store_; }

  int64_t id() const { return metadata_.id; }
  const base::string16& name() const { return metadata_.name; }
  const url::Origin& origin() const { return identifier_.first; }

  void AddObjectStore(blink::IndexedDBObjectStoreMetadata metadata,
                      int64_t new_max_object_store_id);
  blink::IndexedDBObjectStoreMetadata RemoveObjectStore(
      int64_t object_store_id);
  void AddIndex(int64_t object_store_id,
                blink::IndexedDBIndexMetadata metadata,
                int64_t new_max_index_id);
  blink::IndexedDBIndexMetadata RemoveIndex(int64_t object_store_id,
                                            int64_t index_id);

  void ScheduleOpenConnection(
      IndexedDBOriginStateHandle origin_state_handle,
      std::unique_ptr<IndexedDBPendingConnection> connection);
  void ScheduleDeleteDatabase(IndexedDBOriginStateHandle origin_state_handle,
                              scoped_refptr<IndexedDBCallbacks> callbacks,
                              base::OnceClosure on_deletion_complete);

  const blink::IndexedDBDatabaseMetadata& metadata() const { return metadata_; }

  void CreateObjectStore(IndexedDBTransaction* transaction,
                         int64_t object_store_id,
                         const base::string16& name,
                         const blink::IndexedDBKeyPath& key_path,
                         bool auto_increment);
  void DeleteObjectStore(IndexedDBTransaction* transaction,
                         int64_t object_store_id);
  void RenameObjectStore(IndexedDBTransaction* transaction,
                         int64_t object_store_id,
                         const base::string16& new_name);

  // TODO(dmurph): Remove this method and have transactions be directly
  // scheduled using the lock manager.
  void RegisterAndScheduleTransaction(IndexedDBTransaction* transaction);

  // The database object (this object) must be kept alive for the duration of
  // this call. This means the caller should own an IndexedDBOriginStateHandle
  // while caling this methods.
  void ForceClose();

  void Commit(IndexedDBTransaction* transaction);

  void CreateIndex(IndexedDBTransaction* transaction,
                   int64_t object_store_id,
                   int64_t index_id,
                   const base::string16& name,
                   const blink::IndexedDBKeyPath& key_path,
                   bool unique,
                   bool multi_entry);
  void DeleteIndex(IndexedDBTransaction* transaction,
                   int64_t object_store_id,
                   int64_t index_id);
  void RenameIndex(IndexedDBTransaction* transaction,
                   int64_t object_store_id,
                   int64_t index_id,
                   const base::string16& new_name);

  ScopesLockManager* transaction_lock_manager() { return lock_manager_; }
  const ScopesLockManager* transaction_lock_manager() const {
    return lock_manager_;
  }

  void TransactionCreated();
  void TransactionFinished(blink::mojom::IDBTransactionMode mode,
                           bool committed);

  void AbortAllTransactionsForConnections();

  void AddPendingObserver(IndexedDBTransaction* transaction,
                          int32_t observer_id,
                          const IndexedDBObserver::Options& options);

  // |value| can be null for delete and clear operations.
  void FilterObservation(IndexedDBTransaction*,
                         int64_t object_store_id,
                         blink::mojom::IDBOperationType type,
                         const blink::IndexedDBKeyRange& key_range,
                         const IndexedDBValue* value);
  void SendObservations(
      std::map<int32_t, blink::mojom::IDBObserverChangesPtr> change_map);

  void Get(base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
           IndexedDBTransaction* transaction,
           int64_t object_store_id,
           int64_t index_id,
           std::unique_ptr<blink::IndexedDBKeyRange> key_range,
           bool key_only,
           blink::mojom::IDBDatabase::GetCallback callback);
  void GetAll(base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
              IndexedDBTransaction* transaction,
              int64_t object_store_id,
              int64_t index_id,
              std::unique_ptr<blink::IndexedDBKeyRange> key_range,
              bool key_only,
              int64_t max_count,
              blink::mojom::IDBDatabase::GetAllCallback callback);
  void Put(IndexedDBTransaction* transaction,
           int64_t object_store_id,
           IndexedDBValue* value,
           std::unique_ptr<blink::IndexedDBKey> key,
           blink::mojom::IDBPutMode mode,
           scoped_refptr<IndexedDBCallbacks> callbacks,
           const std::vector<blink::IndexedDBIndexKeys>& index_keys);
  void SetIndexKeys(IndexedDBTransaction* transaction,
                    int64_t object_store_id,
                    std::unique_ptr<blink::IndexedDBKey> primary_key,
                    const std::vector<blink::IndexedDBIndexKeys>& index_keys);
  void SetIndexesReady(IndexedDBTransaction* transaction,
                       int64_t object_store_id,
                       const std::vector<int64_t>& index_ids);
  void OpenCursor(IndexedDBTransaction* transaction,
                  int64_t object_store_id,
                  int64_t index_id,
                  std::unique_ptr<blink::IndexedDBKeyRange> key_range,
                  blink::mojom::IDBCursorDirection,
                  bool key_only,
                  blink::mojom::IDBTaskType task_type,
                  scoped_refptr<IndexedDBCallbacks> callbacks);
  void Count(IndexedDBTransaction* transaction,
             int64_t object_store_id,
             int64_t index_id,
             std::unique_ptr<blink::IndexedDBKeyRange> key_range,
             scoped_refptr<IndexedDBCallbacks> callbacks);
  void DeleteRange(IndexedDBTransaction* transaction,
                   int64_t object_store_id,
                   std::unique_ptr<blink::IndexedDBKeyRange> key_range,
                   scoped_refptr<IndexedDBCallbacks> callbacks);
  void GetKeyGeneratorCurrentNumber(
      IndexedDBTransaction* transaction,
      int64_t object_store_id,
      scoped_refptr<IndexedDBCallbacks> callbacks);
  void Clear(IndexedDBTransaction* transaction,
             int64_t object_store_id,
             scoped_refptr<IndexedDBCallbacks> callbacks);

  // Number of connections that have progressed passed initial open call.
  size_t ConnectionCount() const { return connections_.size(); }

  // Number of active open/delete calls (running or blocked on other
  // connections).
  size_t ActiveOpenDeleteCount() const { return active_request_ ? 1 : 0; }

  // Number of open/delete calls that are waiting their turn.
  size_t PendingOpenDeleteCount() const { return pending_requests_.size(); }

  // Asynchronous tasks scheduled within transactions:
  void CreateObjectStoreAbortOperation(int64_t object_store_id);

  leveldb::Status DeleteObjectStoreOperation(int64_t object_store_id,
                                             IndexedDBTransaction* transaction);
  void DeleteObjectStoreAbortOperation(
      blink::IndexedDBObjectStoreMetadata object_store_metadata);
  void RenameObjectStoreAbortOperation(int64_t object_store_id,
                                       base::string16 old_name);
  leveldb::Status VersionChangeOperation(
      int64_t version,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      IndexedDBTransaction* transaction);
  void VersionChangeAbortOperation(int64_t previous_version);
  leveldb::Status DeleteIndexOperation(int64_t object_store_id,
                                       int64_t index_id,
                                       IndexedDBTransaction* transaction);
  void CreateIndexAbortOperation(int64_t object_store_id, int64_t index_id);
  void DeleteIndexAbortOperation(int64_t object_store_id,
                                 blink::IndexedDBIndexMetadata index_metadata);
  void RenameIndexAbortOperation(int64_t object_store_id,
                                 int64_t index_id,
                                 base::string16 old_name);
  leveldb::Status GetOperation(
      base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
      int64_t object_store_id,
      int64_t index_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      indexed_db::CursorType cursor_type,
      blink::mojom::IDBDatabase::GetCallback callback,
      IndexedDBTransaction* transaction);
  leveldb::Status GetAllOperation(
      base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
      int64_t object_store_id,
      int64_t index_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      indexed_db::CursorType cursor_type,
      int64_t max_count,
      blink::mojom::IDBDatabase::GetAllCallback callback,
      IndexedDBTransaction* transaction);
  struct PutOperationParams;
  leveldb::Status PutOperation(std::unique_ptr<PutOperationParams> params,
                               IndexedDBTransaction* transaction);
  leveldb::Status SetIndexesReadyOperation(size_t index_count,
                                           IndexedDBTransaction* transaction);
  struct OpenCursorOperationParams;
  leveldb::Status OpenCursorOperation(
      std::unique_ptr<OpenCursorOperationParams> params,
      IndexedDBTransaction* transaction);
  leveldb::Status CountOperation(
      int64_t object_store_id,
      int64_t index_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      IndexedDBTransaction* transaction);
  leveldb::Status DeleteRangeOperation(
      int64_t object_store_id,
      std::unique_ptr<blink::IndexedDBKeyRange> key_range,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      IndexedDBTransaction* transaction);
  leveldb::Status GetKeyGeneratorCurrentNumberOperation(
      int64_t object_store_id,
      scoped_refptr<IndexedDBCallbacks> callbacks,
      IndexedDBTransaction* transaction);
  leveldb::Status ClearOperation(int64_t object_store_id,
                                 scoped_refptr<IndexedDBCallbacks> callbacks,
                                 IndexedDBTransaction* transaction);

  const list_set<IndexedDBConnection*>& connections() const {
    return connections_;
  }

  base::WeakPtr<IndexedDBDatabase> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 protected:
  friend class IndexedDBTransaction;

  IndexedDBDatabase(const base::string16& name,
                    IndexedDBBackingStore* backing_store,
                    IndexedDBFactory* factory,
                    IndexedDBClassFactory* class_factory,
                    ErrorCallback error_callback,
                    base::OnceClosure destroy_me,
                    std::unique_ptr<IndexedDBMetadataCoding> metadata_coding,
                    const Identifier& unique_identifier,
                    ScopesLockManager* transaction_lock_manager);

  // May be overridden in tests.
  virtual size_t GetUsableMessageSizeInBytes() const;

 private:
  friend class MockBrowserTestIndexedDBClassFactory;
  friend class IndexedDBClassFactory;

  FRIEND_TEST_ALL_PREFIXES(IndexedDBDatabaseTest, OpenDeleteClear);

  void CallUpgradeTransactionStartedForTesting(int64_t old_version);

  class ConnectionRequest;
  class OpenRequest;
  class DeleteRequest;

  leveldb::Status OpenInternal();

  // Called internally when an open or delete request comes in. Processes
  // the queue immediately if there are no other requests.
  void AppendRequest(std::unique_ptr<ConnectionRequest> request);

  // Called by requests when complete. The request will be freed, so the
  // request must do no other work after calling this. If there are pending
  // requests, the queue will be synchronously processed.
  void RequestComplete(ConnectionRequest* request);

  // If there is no active request, grab a new one from the pending queue and
  // start it. Afterwards, possibly release the database by calling
  // MaybeReleaseDatabase().
  void ProcessRequestQueueAndMaybeRelease();

  // If there are no connections, pending requests, or an active request, then
  // this function will call |destroy_me_|, which can destruct this object.
  void MaybeReleaseDatabase();

  std::unique_ptr<IndexedDBConnection> CreateConnection(
      IndexedDBOriginStateHandle origin_state_handle,
      scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks,
      int child_process_id);

  // Ack that one of the connections notified with a "versionchange" event did
  // not promptly close. Therefore a "blocked" event should be fired at the
  // pending connection.
  void VersionChangeIgnored();

  bool HasNoConnections() const;

  void SendVersionChangeToAllConnections(int64_t old_version,
                                         int64_t new_version);

  // This can only be called when the given connection is closed and no longer
  // has any transaction objects.
  void ConnectionClosed(IndexedDBConnection* connection);

  bool ValidateObjectStoreId(int64_t object_store_id) const;
  bool ValidateObjectStoreIdAndIndexId(int64_t object_store_id,
                                       int64_t index_id) const;
  bool ValidateObjectStoreIdAndOptionalIndexId(int64_t object_store_id,
                                               int64_t index_id) const;
  bool ValidateObjectStoreIdAndNewIndexId(int64_t object_store_id,
                                          int64_t index_id) const;

  // Safe because the IndexedDBBackingStore is owned by the same object which
  // owns us, the IndexedDBPerOriginFactory.
  IndexedDBBackingStore* backing_store_;
  blink::IndexedDBDatabaseMetadata metadata_;

  const Identifier identifier_;
  // TODO(dmurph): Remove the need for this to be here (and then remove it).
  IndexedDBFactory* factory_;
  IndexedDBClassFactory* const class_factory_;
  std::unique_ptr<IndexedDBMetadataCoding> metadata_coding_;

  ScopesLockManager* lock_manager_;
  int64_t transaction_count_ = 0;

  // Called when a backing store operation has failed. The database will be
  // closed (IndexedDBFactory::ForceClose) during this call. This should NOT
  // be used in an method scheduled as a transaction operation.
  ErrorCallback error_callback_;

  // Calling this closure will destroy this object.
  base::OnceClosure destroy_me_;

  list_set<IndexedDBConnection*> connections_;

  // During ForceClose(), the internal state can be inconsistent during cleanup,
  // specifically for ConnectionClosed() and MaybeReleaseDatabase(). Keeping
  // track of whether the code is currently in the ForceClose() method helps
  // ensure that the state stays consistent.
  bool force_closing_ = false;

  // This holds the first open or delete request that is currently being
  // processed. The request has already broadcast OnVersionChange if
  // necessary.
  std::unique_ptr<ConnectionRequest> active_request_;

  // This holds open or delete requests that are waiting for the active
  // request to be completed. The requests have not yet broadcast
  // OnVersionChange (if necessary).
  base::queue<std::unique_ptr<ConnectionRequest>> pending_requests_;

  // The |processing_pending_requests_| flag is set while ProcessRequestQueue()
  // is executing. It prevents rentrant calls if the active request completes
  // synchronously.
  bool processing_pending_requests_ = false;

  // |weak_factory_| is used for all callback uses.
  base::WeakPtrFactory<IndexedDBDatabase> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBDatabase);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DATABASE_H_
