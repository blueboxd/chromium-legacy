// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host_observer.h"
#include "mojo/public/cpp/bindings/associated_binding_set.h"
#include "mojo/public/cpp/bindings/strong_associated_binding_set.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace base {
class SequencedTaskRunner;
}

namespace url {
class Origin;
}

namespace content {
class CursorImpl;
class IndexedDBContextImpl;
class IndexedDBTransaction;

// Constructed on UI thread.  All remaining calls (including destruction) should
// happen on the IDB sequenced task runner.
class CONTENT_EXPORT IndexedDBDispatcherHost
    : public blink::mojom::IDBFactory,
      public RenderProcessHostObserver {
 public:
  // Only call the constructor from the UI thread.
  IndexedDBDispatcherHost(
      int ipc_process_id,
      scoped_refptr<IndexedDBContextImpl> indexed_db_context,
      scoped_refptr<ChromeBlobStorageContext> blob_storage_context);

  void AddBinding(blink::mojom::IDBFactoryRequest request,
                  const url::Origin& origin);

  void AddDatabaseBinding(std::unique_ptr<blink::mojom::IDBDatabase> database,
                          blink::mojom::IDBDatabaseAssociatedRequest request);

  void AddCursorBinding(std::unique_ptr<CursorImpl> cursor,
                        blink::mojom::IDBCursorAssociatedRequest request);
  void RemoveCursorBinding(mojo::BindingId binding_id);

  void AddTransactionBinding(
      std::unique_ptr<blink::mojom::IDBTransaction> transaction,
      blink::mojom::IDBTransactionAssociatedRequest request);

  // A shortcut for accessing our context.
  IndexedDBContextImpl* context() const { return indexed_db_context_.get(); }
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context() const {
    return blob_storage_context_;
  }
  int ipc_process_id() const { return ipc_process_id_; }

  // Must be called on the IDB sequence.
  base::WeakPtr<IndexedDBDispatcherHost> AsWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void CreateAndBindTransactionImpl(
      blink::mojom::IDBTransactionAssociatedRequest transaction_request,
      const url::Origin& origin,
      base::WeakPtr<IndexedDBTransaction> transaction);

  // Called by UI thread. Used to kill outstanding bindings and weak pointers
  // in callbacks.
  void RenderProcessExited(RenderProcessHost* host,
                           const ChildProcessTerminationInfo& info) override;

 private:
  class IDBSequenceHelper;
  // Friends to enable OnDestruct() delegation.
  friend class BrowserThread;
  friend class IndexedDBDispatcherHostTest;
  friend class base::DeleteHelper<IndexedDBDispatcherHost>;

  ~IndexedDBDispatcherHost() override;

  // blink::mojom::IDBFactory implementation:
  void GetDatabaseInfo(
      blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) override;
  void GetDatabaseNames(
      blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) override;
  void Open(blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info,
            blink::mojom::IDBDatabaseCallbacksAssociatedPtrInfo
                database_callbacks_info,
            const base::string16& name,
            int64_t version,
            blink::mojom::IDBTransactionAssociatedRequest transaction_request,
            int64_t transaction_id) override;
  void DeleteDatabase(
      blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info,
      const base::string16& name,
      bool force_close) override;
  void AbortTransactionsAndCompactDatabase(
      AbortTransactionsAndCompactDatabaseCallback callback) override;
  void AbortTransactionsForDatabase(
      AbortTransactionsForDatabaseCallback callback) override;

  void InvalidateWeakPtrsAndClearBindings();

  base::SequencedTaskRunner* IDBTaskRunner() const;

  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Used to set file permissions for blob storage.
  const int ipc_process_id_;

  // State for each client held in |bindings_|.
  struct BindingState {
    url::Origin origin;
  };

  mojo::BindingSet<blink::mojom::IDBFactory, BindingState> bindings_;
  mojo::StrongAssociatedBindingSet<blink::mojom::IDBDatabase>
      database_bindings_;
  mojo::StrongAssociatedBindingSet<blink::mojom::IDBCursor> cursor_bindings_;
  mojo::StrongAssociatedBindingSet<blink::mojom::IDBTransaction>
      transaction_bindings_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<IndexedDBDispatcherHost> weak_factory_{this};

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
