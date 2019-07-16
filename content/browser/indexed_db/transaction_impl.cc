// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/transaction_impl.h"

#include <utility>
#include <vector>

#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/platform/modules/indexeddb/web_idb_database_exception.h"

namespace content {
namespace {
const char kInvalidBlobUuid[] = "Blob does not exist";
const char kInvalidBlobFilePath[] = "Blob file path is invalid";

void LogUMAPutBlobCount(size_t blob_count) {
  UMA_HISTOGRAM_COUNTS_1000("WebCore.IndexedDB.PutBlobsCount", blob_count);
}

}  // namespace

// Expect to be created on IDB sequence and called/destroyed on IO thread.
class TransactionImpl::IOHelper {
 public:
  enum class LoadResultCode {
    kNoop,
    kAbort,
    kInvalidBlobPath,
    kSuccess,
  };

  struct LoadResult {
    LoadResultCode code;
    blink::mojom::IDBValuePtr value;
    std::vector<IndexedDBBlobInfo> blob_info;
  };

  IOHelper(base::SequencedTaskRunner* idb_runner,
           scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
           int64_t ipc_process_id);
  ~IOHelper();

  void LoadBlobsOnIOThread(blink::mojom::IDBValuePtr value,
                           base::WaitableEvent* signal_when_finished,
                           LoadResult* result);

 private:
  // Friends to enable OnDestruct() delegation.
  friend class BrowserThread;
  friend class base::DeleteHelper<TransactionImpl::IOHelper>;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;
  int64_t ipc_process_id_;
  SEQUENCE_CHECKER(sequence_checker_);
};

TransactionImpl::TransactionImpl(
    base::WeakPtr<IndexedDBTransaction> transaction,
    const url::Origin& origin,
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : io_helper_(new IOHelper(idb_runner.get(),
                              dispatcher_host->blob_storage_context(),
                              dispatcher_host->ipc_process_id())),
      dispatcher_host_(dispatcher_host),
      indexed_db_context_(dispatcher_host->context()),
      transaction_(std::move(transaction)),
      origin_(origin),
      idb_runner_(std::move(idb_runner)) {
  DCHECK(idb_runner_->RunsTasksInCurrentSequence());
  DCHECK(dispatcher_host_);
  DCHECK(transaction_);
}

TransactionImpl::~TransactionImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TransactionImpl::CreateObjectStore(int64_t object_store_id,
                                        const base::string16& name,
                                        const blink::IndexedDBKeyPath& key_path,
                                        bool auto_increment) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transaction_)
    return;

  if (transaction_->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "CreateObjectStore must be called from a version change transaction.");
    return;
  }

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected())
    return;

  // Note: This doesn't schedule a task on the transaction because the
  // SetIndexKeys call path isn't asynchronous.
  connection->database()->CreateObjectStore(transaction_.get(), object_store_id,
                                            name, key_path, auto_increment);
}

void TransactionImpl::DeleteObjectStore(int64_t object_store_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transaction_)
    return;

  if (transaction_->mode() != blink::mojom::IDBTransactionMode::VersionChange) {
    mojo::ReportBadMessage(
        "DeleteObjectStore must be called from a version change transaction.");
    return;
  }

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected())
    return;

  connection->database()->DeleteObjectStore(transaction_.get(),
                                            object_store_id);
}

void TransactionImpl::Put(
    int64_t object_store_id,
    blink::mojom::IDBValuePtr value_ptr,
    const blink::IndexedDBKey& key,
    blink::mojom::IDBPutMode mode,
    const std::vector<blink::IndexedDBIndexKeys>& index_keys,
    blink::mojom::IDBCallbacksAssociatedPtrInfo callbacks_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(dispatcher_host_);

  IOHelper::LoadResult result;
  if (value_ptr->blob_or_file_info.empty()) {
    // If there are no blobs to process, we don't need to hop to the IO thread
    // to load blobs.
    result.code = IOHelper::LoadResultCode::kSuccess;
    result.value = std::move(value_ptr);
    result.blob_info = std::vector<IndexedDBBlobInfo>();
    LogUMAPutBlobCount(result.blob_info.size());
  } else {
    // TODO(crbug.com/932869): Remove IO thread hop entirely.
    base::WaitableEvent signal_when_finished(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);
    // |io_helper_| is owned by |this| and this call is synchronized with a
    // WaitableEvent, so |io_helper_| is guaranteed to remain alive throughout
    // the duration of the LoadBlobsOnIOThread() invocation.
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&TransactionImpl::IOHelper::LoadBlobsOnIOThread,
                       base::Unretained(io_helper_.get()), std::move(value_ptr),
                       &signal_when_finished, &result));
    signal_when_finished.Wait();
  }

  switch (result.code) {
    case IOHelper::LoadResultCode::kNoop:
      return;
    case IOHelper::LoadResultCode::kAbort: {
      IndexedDBDatabaseError error(blink::kWebIDBDatabaseExceptionUnknownError,
                                   kInvalidBlobUuid);
      scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
          dispatcher_host_->AsWeakPtr(), origin_, std::move(callbacks_info),
          dispatcher_host_->context()->TaskRunner()));

      callbacks->OnError(error);

      if (!transaction_)
        return;

      IndexedDBConnection* connection = transaction_->connection();
      if (!connection->IsConnected())
        return;

      connection->AbortTransaction(transaction_.get(), error);
      return;
    }
    case IOHelper::LoadResultCode::kInvalidBlobPath: {
      mojo::ReportBadMessage(kInvalidBlobFilePath);
      return;
    }
    case IOHelper::LoadResultCode::kSuccess: {
      if (!transaction_)
        return;

      IndexedDBConnection* connection = transaction_->connection();
      if (!connection->IsConnected())
        return;

      // Value size recorded in IDBObjectStore before we can auto-wrap in a
      // blob. 1KB to 10MB.
      UMA_HISTOGRAM_COUNTS_10000("WebCore.IndexedDB.PutKeySize",
                                 key.size_estimate() / 1024);

      uint64_t commit_size = result.value->bits.size() + key.size_estimate();
      IndexedDBValue value;
      // TODO(crbug.com/902498): Use mojom traits to map directly to
      // std::string.
      value.bits =
          std::string(result.value->bits.begin(), result.value->bits.end());
      // Release result.value->bits std::vector.
      result.value->bits.clear();
      swap(value.blob_info, result.blob_info);
      scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
          dispatcher_host_->AsWeakPtr(), origin_, std::move(callbacks_info),
          dispatcher_host_->context()->TaskRunner()));
      connection->database()->Put(transaction_.get(), object_store_id, &value,
                                  std::make_unique<blink::IndexedDBKey>(key),
                                  mode, std::move(callbacks), index_keys);

      // Size can't be big enough to overflow because it represents the
      // actual bytes passed through IPC.
      transaction_->set_size(transaction_->size() + commit_size);
      return;
    }
  }
}

TransactionImpl::IOHelper::IOHelper(
    base::SequencedTaskRunner* idb_runner,
    scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
    int64_t ipc_process_id)
    : blob_storage_context_(blob_storage_context),
      ipc_process_id_(ipc_process_id) {
  DCHECK(idb_runner->RunsTasksInCurrentSequence());
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

TransactionImpl::IOHelper::~IOHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void TransactionImpl::IOHelper::LoadBlobsOnIOThread(
    blink::mojom::IDBValuePtr value,
    base::WaitableEvent* signal_when_finished,
    LoadResult* result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::ScopedClosureRunner signal_runner(
      base::BindOnce([](base::WaitableEvent* signal) { signal->Signal(); },
                     signal_when_finished));

  if (!blob_storage_context_) {
    result->code = IOHelper::LoadResultCode::kNoop;
    return;
  }

  // Should only be called if there are blobs to process.
  CHECK(!value->blob_or_file_info.empty());

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();

  base::CheckedNumeric<uint64_t> total_blob_size = 0;
  std::vector<IndexedDBBlobInfo> blob_info(value->blob_or_file_info.size());
  for (size_t i = 0; i < value->blob_or_file_info.size(); ++i) {
    blink::mojom::IDBBlobInfoPtr& info = value->blob_or_file_info[i];

    std::unique_ptr<storage::BlobDataHandle> handle =
        blob_storage_context_->context()->GetBlobDataFromUUID(info->uuid);

    // Due to known issue crbug.com/351753, blobs can die while being passed to
    // a different process. So this case must be handled gracefully.
    // TODO(dmurph): Revert back to using mojo::ReportBadMessage once fixed.
    UMA_HISTOGRAM_BOOLEAN("Storage.IndexedDB.PutValidBlob",
                          handle.get() != nullptr);
    if (!handle) {
      result->code = LoadResultCode::kAbort;
      return;
    }
    uint64_t size = handle->size();
    UMA_HISTOGRAM_MEMORY_KB("Storage.IndexedDB.PutBlobSizeKB", size / 1024ull);
    total_blob_size += size;

    if (info->file) {
      if (!info->file->path.empty() &&
          !policy->CanReadFile(ipc_process_id_, info->file->path)) {
        result->code = LoadResultCode::kInvalidBlobPath;
        return;
      }
      blob_info[i] = IndexedDBBlobInfo(std::move(handle), info->file->path,
                                       info->file->name, info->mime_type);
      if (info->size != -1) {
        blob_info[i].set_last_modified(info->file->last_modified);
        blob_info[i].set_size(info->size);
      }
    } else {
      blob_info[i] =
          IndexedDBBlobInfo(std::move(handle), info->mime_type, info->size);
    }
  }
  LogUMAPutBlobCount(blob_info.size());
  uint64_t blob_size = total_blob_size.ValueOrDefault(0U);
  if (blob_size != 0) {
    // Bytes to kilobytes.
    UMA_HISTOGRAM_COUNTS_1M("WebCore.IndexedDB.PutBlobsTotalSize",
                            blob_size / 1024);
  }
  result->code = LoadResultCode::kSuccess;
  result->value = std::move(value);
  result->blob_info = std::move(blob_info);
}

void TransactionImpl::Commit(int64_t num_errors_handled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transaction_)
    return;

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected())
    return;

  transaction_->SetNumErrorsHandled(num_errors_handled);

  // Always allow empty or delete-only transactions.
  if (transaction_->size() == 0) {
    connection->database()->Commit(transaction_.get());
    return;
  }

  indexed_db_context_->quota_manager_proxy()->GetUsageAndQuota(
      indexed_db_context_->TaskRunner(), origin_,
      blink::mojom::StorageType::kTemporary,
      base::BindOnce(&TransactionImpl::OnGotUsageAndQuotaForCommit,
                     weak_factory_.GetWeakPtr()));
}

void TransactionImpl::OnGotUsageAndQuotaForCommit(
    blink::mojom::QuotaStatusCode status,
    int64_t usage,
    int64_t quota) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!transaction_)
    return;

  // May have disconnected while quota check was pending.
  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected())
    return;

  if (status == blink::mojom::QuotaStatusCode::kOk &&
      usage + transaction_->size() <= quota) {
    connection->database()->Commit(transaction_.get());
  } else {
    connection->AbortTransaction(
        transaction_.get(),
        IndexedDBDatabaseError(blink::kWebIDBDatabaseExceptionQuotaError));
  }
}

}  // namespace content
