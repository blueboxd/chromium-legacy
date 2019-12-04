// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/transaction_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/indexed_db/indexed_db_callback_helpers.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/browser/indexed_db/indexed_db_factory_impl.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"

namespace content {
namespace {
const char kInvalidBlobFilePath[] = "Blob file path is invalid";

IndexedDBDatabaseError CreateBackendAbortError() {
  return IndexedDBDatabaseError(blink::mojom::IDBException::kAbortError,
                                "Backend aborted error");
}

}  // namespace

TransactionImpl::TransactionImpl(
    base::WeakPtr<IndexedDBTransaction> transaction,
    const url::Origin& origin,
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : dispatcher_host_(dispatcher_host),
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

  transaction_->ScheduleTask(
      blink::mojom::IDBTaskType::Preemptive,
      BindWeakOperation(&IndexedDBDatabase::CreateObjectStoreOperation,
                        connection->database()->AsWeakPtr(), object_store_id,
                        name, key_path, auto_increment));
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

  if (!connection->database()->IsObjectStoreIdInMetadata(object_store_id))
    return;

  transaction_->ScheduleTask(
      BindWeakOperation(&IndexedDBDatabase::DeleteObjectStoreOperation,
                        connection->database()->AsWeakPtr(), object_store_id));
}

void TransactionImpl::Put(
    int64_t object_store_id,
    blink::mojom::IDBValuePtr input_value,
    const blink::IndexedDBKey& key,
    blink::mojom::IDBPutMode mode,
    const std::vector<blink::IndexedDBIndexKeys>& index_keys,
    blink::mojom::IDBTransaction::PutCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(dispatcher_host_);

  std::vector<IndexedDBBlobInfo> blob_infos;
  if (!input_value->blob_or_file_info.empty()) {
    bool security_policy_failure = false;
    CreateBlobInfos(input_value, &blob_infos, &security_policy_failure);
    if (security_policy_failure) {
      IndexedDBDatabaseError error = CreateBackendAbortError();
      std::move(callback).Run(
          blink::mojom::IDBTransactionPutResult::NewErrorResult(
              blink::mojom::IDBError::New(error.code(), error.message())));
      mojo::ReportBadMessage(kInvalidBlobFilePath);
      return;
    }
  }

  if (!transaction_) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Unknown transaction.");
    std::move(callback).Run(
        blink::mojom::IDBTransactionPutResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  IndexedDBConnection* connection = transaction_->connection();
  if (!connection->IsConnected()) {
    IndexedDBDatabaseError error(blink::mojom::IDBException::kUnknownError,
                                 "Not connected.");
    std::move(callback).Run(
        blink::mojom::IDBTransactionPutResult::NewErrorResult(
            blink::mojom::IDBError::New(error.code(), error.message())));
    return;
  }

  uint64_t commit_size = input_value->bits.size() + key.size_estimate();
  std::unique_ptr<IndexedDBDatabase::PutOperationParams> params(
      std::make_unique<IndexedDBDatabase::PutOperationParams>());
  IndexedDBValue& output_value = params->value;

  // TODO(crbug.com/902498): Use mojom traits to map directly to
  // std::string.
  output_value.bits =
      std::string(input_value->bits.begin(), input_value->bits.end());
  // Release value->bits std::vector.
  input_value->bits.clear();
  swap(output_value.blob_info, blob_infos);

  blink::mojom::IDBTransaction::PutCallback aborting_callback =
      CreateCallbackAbortOnDestruct<blink::mojom::IDBTransaction::PutCallback,
                                    blink::mojom::IDBTransactionPutResultPtr>(
          std::move(callback), transaction_->AsWeakPtr());

  params->object_store_id = object_store_id;
  params->key = std::make_unique<blink::IndexedDBKey>(key);
  params->put_mode = mode;
  params->callback = std::move(aborting_callback);
  params->index_keys = index_keys;
  transaction_->ScheduleTask(BindWeakOperation(
      &IndexedDBDatabase::PutOperation, connection->database()->AsWeakPtr(),
      std::move(params)));

  // Size can't be big enough to overflow because it represents the
  // actual bytes passed through IPC.
  transaction_->set_size(transaction_->size() + commit_size);
}

void TransactionImpl::CreateBlobInfos(
    blink::mojom::IDBValuePtr& value,
    std::vector<IndexedDBBlobInfo>* blob_infos,
    bool* security_policy_failure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  *security_policy_failure = false;

  // Should only be called if there are blobs to process.
  CHECK(!value->blob_or_file_info.empty());

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  int64_t ipc_process_id = dispatcher_host_->ipc_process_id();

  base::CheckedNumeric<uint64_t> total_blob_size = 0;
  blob_infos->resize(value->blob_or_file_info.size());
  for (size_t i = 0; i < value->blob_or_file_info.size(); ++i) {
    blink::mojom::IDBBlobInfoPtr& info = value->blob_or_file_info[i];
    uint64_t size = info->size;
    total_blob_size += size;

    if (info->file) {
      if (!info->file->path.empty() &&
          !policy->CanReadFile(ipc_process_id, info->file->path)) {
        blob_infos->clear();
        *security_policy_failure = true;
        return;
      }
      (*blob_infos)[i] =
          IndexedDBBlobInfo(std::move(info->blob), info->uuid, info->file->path,
                            info->file->name, info->mime_type);
      if (info->size != -1) {
        (*blob_infos)[i].set_last_modified(info->file->last_modified);
        (*blob_infos)[i].set_size(info->size);
      }
    } else {
      (*blob_infos)[i] = IndexedDBBlobInfo(std::move(info->blob), info->uuid,
                                           info->mime_type, info->size);
    }
  }
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
    connection->AbortTransactionAndTearDownOnError(
        transaction_.get(),
        IndexedDBDatabaseError(blink::mojom::IDBException::kQuotaError));
  }
}

}  // namespace content
