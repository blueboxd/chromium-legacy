// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_COORDINATOR_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_COORDINATOR_H_

#include <memory>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/indexed_db/indexed_db_origin_state_handle.h"
#include "content/browser/indexed_db/list_set.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
class IndexedDBCallbacks;
class IndexedDBConnection;
class IndexedDBDatabase;
struct IndexedDBPendingConnection;

class IndexedDBConnectionCoordinator {
 public:
  static const int64_t kInvalidId = 0;
  static const int64_t kMinimumIndexId = 30;

  explicit IndexedDBConnectionCoordinator(IndexedDBDatabase* db);
  ~IndexedDBConnectionCoordinator();

  void ScheduleOpenConnection(
      IndexedDBOriginStateHandle origin_state_handle,
      std::unique_ptr<IndexedDBPendingConnection> connection);

  void ScheduleDeleteDatabase(IndexedDBOriginStateHandle origin_state_handle,
                              scoped_refptr<IndexedDBCallbacks> callbacks,
                              base::OnceClosure on_deletion_complete);

  // Returns if the caller should process the request queue.
  void ForceClose();

  void OnConnectionClosed(IndexedDBConnection* connection);

  // Ack that one of the connections notified with a "versionchange" event did
  // not promptly close. Therefore a "blocked" event should be fired at the
  // pending connection.
  void OnVersionChangeIgnored();

  void CreateAndBindUpgradeTransaction();

  void OnUpgradeTransactionStarted(int64_t old_version);

  void OnUpgradeTransactionFinished(bool committed);

  // If there is no active request, grab a new one from the pending queue and
  // start it. Afterwards, possibly release the database by calling
  // MaybeReleaseDatabase().
  void ProcessRequestQueue();

  bool processing_pending_requests() const {
    return processing_pending_requests_;
  }

  bool force_closing() const { return force_closing_; }

  const list_set<IndexedDBConnection*>& connections() const {
    return connections_;
  }

  void AddConnection(IndexedDBConnection* connection);

  // Number of active open/delete calls (running or blocked on other
  // connections).
  size_t ActiveOpenDeleteCount() const { return active_request_ ? 1 : 0; }

  // Number of open/delete calls that are waiting their turn.
  size_t PendingOpenDeleteCount() const { return pending_requests_.size(); }

  // Number of connections that have progressed passed initial open call.
  size_t ConnectionCount() const { return connections_.size(); }

  bool HasActiveRequest() { return !!active_request_; }

  bool HasPendingRequests() { return !pending_requests_.empty(); }

  base::WeakPtr<IndexedDBConnectionCoordinator> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  friend class IndexedDBDatabase;
  class ConnectionRequest;
  class OpenRequest;
  class DeleteRequest;

  // Called internally when an open or delete request comes in. Processes
  // the queue immediately if there are no other requests.
  void AppendRequest(std::unique_ptr<ConnectionRequest> request);

  // Called by requests when complete. The request will be freed, so the
  // request must do no other work after calling this. If there are pending
  // requests, the queue will be synchronously processed.
  void RequestComplete(ConnectionRequest* request);

  IndexedDBDatabase* db_;

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
  base::WeakPtrFactory<IndexedDBConnectionCoordinator> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IndexedDBConnectionCoordinator);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CONNECTION_COORDINATOR_H_
