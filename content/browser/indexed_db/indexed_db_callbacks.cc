// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_callbacks.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/indexed_db/cursor_impl.h"
#include "content/browser/indexed_db/database_impl.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_return_value.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/indexed_db_transaction.h"
#include "content/browser/indexed_db/indexed_db_value.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/common/indexeddb/indexeddb_metadata.h"

using blink::IndexedDBDatabaseMetadata;
using blink::IndexedDBKey;
using std::swap;

namespace content {

namespace {

// The following two objects protect the given objects from being destructed
// while the current transaction task queue is being processed.
class SafeConnectionWrapper {
 public:
  explicit SafeConnectionWrapper(
      std::unique_ptr<IndexedDBConnection> connection)
      : connection_(std::move(connection)),
        idb_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  ~SafeConnectionWrapper() {
    if (connection_) {
      idb_runner_->PostTask(
          FROM_HERE, base::BindOnce(
                         [](std::unique_ptr<IndexedDBConnection> connection) {
                           connection->CloseAndReportForceClose();
                         },
                         std::move(connection_)));
    }
  }
  SafeConnectionWrapper(SafeConnectionWrapper&& other) = default;

  std::unique_ptr<IndexedDBConnection> connection_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeConnectionWrapper);
};

class SafeCursorWrapper {
 public:
  explicit SafeCursorWrapper(std::unique_ptr<IndexedDBCursor> cursor)
      : cursor_(std::move(cursor)),
        idb_runner_(base::SequencedTaskRunnerHandle::Get()) {}
  ~SafeCursorWrapper() {
    if (cursor_)
      idb_runner_->DeleteSoon(FROM_HERE, cursor_.release());
  }
  SafeCursorWrapper(SafeCursorWrapper&& other) = default;

  std::unique_ptr<IndexedDBCursor> cursor_;
  scoped_refptr<base::SequencedTaskRunner> idb_runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SafeCursorWrapper);
};

}  // namespace

IndexedDBCallbacks::IndexedDBValueBlob::IndexedDBValueBlob(
    const IndexedDBBlobInfo& blob_info,
    blink::mojom::IDBBlobInfoPtr* blob_or_file_info)
    : blob_info_(blob_info) {
  if (blob_info_.blob_handle()) {
    uuid_ = blob_info_.blob_handle()->uuid();
  } else {
    uuid_ = base::GenerateGUID();
  }
  (*blob_or_file_info)->uuid = uuid_;
  receiver_ = (*blob_or_file_info)->blob.InitWithNewPipeAndPassReceiver();
}
IndexedDBCallbacks::IndexedDBValueBlob::IndexedDBValueBlob(
    IndexedDBValueBlob&& other) = default;
IndexedDBCallbacks::IndexedDBValueBlob::~IndexedDBValueBlob() = default;

// static
void IndexedDBCallbacks::IndexedDBValueBlob::GetIndexedDBValueBlobs(
    std::vector<IndexedDBValueBlob>* value_blobs,
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<blink::mojom::IDBBlobInfoPtr>* blob_or_file_info) {
  DCHECK(value_blobs);
  DCHECK(blob_or_file_info);
  DCHECK_EQ(blob_info.size(), blob_or_file_info->size());
  value_blobs->reserve(value_blobs->size() + blob_info.size());
  for (size_t i = 0; i < blob_info.size(); i++) {
    value_blobs->push_back(
        IndexedDBValueBlob(blob_info[i], &(*blob_or_file_info)[i]));
  }
}

// static
std::vector<IndexedDBCallbacks::IndexedDBValueBlob>
IndexedDBCallbacks::IndexedDBValueBlob::GetIndexedDBValueBlobs(
    const std::vector<IndexedDBBlobInfo>& blob_info,
    std::vector<blink::mojom::IDBBlobInfoPtr>* blob_or_file_info) {
  std::vector<IndexedDBValueBlob> value_blobs;
  IndexedDBValueBlob::GetIndexedDBValueBlobs(&value_blobs, blob_info,
                                             blob_or_file_info);
  return value_blobs;
}

// static
void IndexedDBCallbacks::CreateAllBlobs(
    IndexedDBDispatcherHost* dispatcher_host,
    std::vector<IndexedDBValueBlob> value_blobs) {
  IDB_TRACE("IndexedDBCallbacks::CreateAllBlobs");

  DCHECK(dispatcher_host);

  if (value_blobs.empty())
    return;

  // First, handle all the "file path" value blobs on this sequence.
  for (auto& blob : value_blobs) {
    DCHECK(blob.receiver_.is_valid());

    auto& blob_info = blob.blob_info_;
    if (blob_info.blob_handle())
      continue;

    auto element = storage::mojom::BlobDataItem::New();
    // TODO(enne): do we have to handle unknown size here??
    element->size = blob_info.size();
    element->side_data_size = 0;
    element->content_type = base::UTF16ToUTF8(blob_info.type());
    element->type = storage::mojom::BlobDataItemType::kIndexedDB;

    dispatcher_host->BindFileReader(
        blob_info.file_path(), blob_info.last_modified(),
        blob_info.release_callback(),
        element->reader.InitWithNewPipeAndPassReceiver());

    dispatcher_host->mojo_blob_storage_context()->RegisterFromDataItem(
        std::move(blob.receiver_), blob.uuid_, std::move(element));
  }

  // TODO(crbug.com/932869): Remove IO thread hop entirely.
  base::WaitableEvent signal_when_finished(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Then, handle all the "blob handle" value blobs on the IO thread,
  // as BlobImpl can only be accessed from there.
  //
  // WARNING: IndexedDBValueBlob holds a const *reference* to its blob_info_
  // and therefore must not outlive the blob info it points to.  This is
  // why the waitable event is required here so that the IO thread task
  // can finish with the value blobs still alive.
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(
                     [](std::vector<IndexedDBValueBlob> value_blobs,
                        base::WaitableEvent* signal) {
                       for (auto& blob : value_blobs) {
                         auto& blob_info = blob.blob_info_;
                         if (!blob_info.blob_handle())
                           continue;

                         // TODO(enne): when blob handle becomes a remote, this
                         // will become a Clone() call.
                         auto blob_data =
                             std::make_unique<storage::BlobDataHandle>(
                                 *blob_info.blob_handle());
                         storage::BlobImpl::Create(std::move(blob_data),
                                                   std::move(blob.receiver_));
                       }
                       signal->Signal();
                     },
                     std::move(value_blobs), &signal_when_finished));
  signal_when_finished.Wait();
}

IndexedDBCallbacks::IndexedDBCallbacks(
    base::WeakPtr<IndexedDBDispatcherHost> dispatcher_host,
    const url::Origin& origin,
    mojo::PendingAssociatedRemote<blink::mojom::IDBCallbacks> pending_callbacks,
    scoped_refptr<base::SequencedTaskRunner> idb_runner)
    : data_loss_(blink::mojom::IDBDataLoss::None),
      dispatcher_host_(std::move(dispatcher_host)),
      origin_(origin),
      idb_runner_(std::move(idb_runner)) {
  DCHECK(idb_runner_->RunsTasksInCurrentSequence());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_callbacks.is_valid()) {
    callbacks_.Bind(std::move(pending_callbacks));
    // |callbacks_| is owned by |this|, so if |this| is destroyed, then
    // |callbacks_| will also be destroyed.  While |callbacks_| is otherwise
    // alive, |this| will always be valid.
    callbacks_.set_disconnect_handler(base::BindOnce(
        &IndexedDBCallbacks::OnConnectionError, base::Unretained(this)));
  }
}

IndexedDBCallbacks::~IndexedDBCallbacks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void IndexedDBCallbacks::OnError(const IndexedDBDatabaseError& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->Error(error.code(), error.message());
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(
    std::vector<blink::mojom::IDBNameAndVersionPtr> names_and_versions) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->SuccessNamesAndVersionsList(std::move(names_and_versions));
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(const std::vector<base::string16>& value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->SuccessStringList(value);
  complete_ = true;
}

void IndexedDBCallbacks::OnBlocked(int64_t existing_version) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (sent_blocked_)
    return;

  sent_blocked_ = true;

  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  if (callbacks_)
    callbacks_->Blocked(existing_version);
}

void IndexedDBCallbacks::OnUpgradeNeeded(
    int64_t old_version,
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata,
    const IndexedDBDataLossInfo& data_loss_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK(!connection_created_);

  data_loss_ = data_loss_info.status;
  connection_created_ = true;

  SafeConnectionWrapper wrapper(std::move(connection));
  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }

  auto database =
      std::make_unique<DatabaseImpl>(std::move(wrapper.connection_), origin_,
                                     dispatcher_host_.get(), idb_runner_);

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_remote;
  dispatcher_host_->AddDatabaseBinding(
      std::move(database), pending_remote.InitWithNewEndpointAndPassReceiver());
  callbacks_->UpgradeNeeded(std::move(pending_remote), old_version,
                            data_loss_info.status, data_loss_info.message,
                            metadata);
}

void IndexedDBCallbacks::OnSuccess(
    std::unique_ptr<IndexedDBConnection> connection,
    const IndexedDBDatabaseMetadata& metadata) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(connection_created_, !connection);

  scoped_refptr<IndexedDBCallbacks> self(this);

  // Only create a new connection if one was not previously sent in
  // OnUpgradeNeeded.
  std::unique_ptr<IndexedDBConnection> database_connection;
  if (!connection_created_)
    database_connection = std::move(connection);

  SafeConnectionWrapper wrapper(std::move(database_connection));
  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }

  mojo::PendingAssociatedRemote<blink::mojom::IDBDatabase> pending_remote;
  if (wrapper.connection_) {
    auto database =
        std::make_unique<DatabaseImpl>(std::move(wrapper.connection_), origin_,
                                       dispatcher_host_.get(), idb_runner_);
    dispatcher_host_->AddDatabaseBinding(
        std::move(database),
        pending_remote.InitWithNewEndpointAndPassReceiver());
  }
  callbacks_->SuccessDatabase(std::move(pending_remote), metadata);
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess(int64_t value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->SuccessInteger(value);
  complete_ = true;
}

void IndexedDBCallbacks::OnSuccess() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!complete_);

  DCHECK_EQ(blink::mojom::IDBDataLoss::None, data_loss_);

  if (!callbacks_)
    return;
  if (!dispatcher_host_) {
    OnConnectionError();
    return;
  }
  callbacks_->Success();
  complete_ = true;
}

void IndexedDBCallbacks::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  callbacks_.reset();
  dispatcher_host_ = nullptr;
}

}  // namespace content
