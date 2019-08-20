// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_file_writer_impl.h"
#include "base/logging.h"
#include "content/browser/native_file_system/native_file_system_error.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"

using blink::mojom::NativeFileSystemStatus;
using storage::BlobDataHandle;
using storage::FileSystemOperation;

namespace content {

struct NativeFileSystemFileWriterImpl::WriteState {
  WriteCallback callback;
  uint64_t bytes_written = 0;
};

NativeFileSystemFileWriterImpl::NativeFileSystemFileWriterImpl(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const storage::FileSystemURL& swap_url,
    const SharedHandleState& handle_state)
    : NativeFileSystemHandleBase(manager,
                                 context,
                                 url,
                                 handle_state,
                                 /*is_directory=*/false),
      swap_url_(swap_url) {}

NativeFileSystemFileWriterImpl::~NativeFileSystemFileWriterImpl() {
  if (can_purge()) {
    manager()->operation_runner()->RemoveFile(
        swap_url(), base::BindOnce(
                        [](const storage::FileSystemURL& swap_url,
                           base::File::Error result) {
                          if (result != base::File::FILE_OK) {
                            DLOG(ERROR) << "Error Deleting Swap File, status: "
                                        << base::File::ErrorToString(result)
                                        << " path: " << swap_url.path();
                          }
                        },
                        swap_url()));
  }
}

void NativeFileSystemFileWriterImpl::Write(uint64_t offset,
                                           blink::mojom::BlobPtr data,
                                           WriteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::WriteImpl,
                     weak_factory_.GetWeakPtr(), offset, std::move(data)),
      base::BindOnce([](WriteCallback callback) {
        std::move(callback).Run(native_file_system_error::FromStatus(
                                    NativeFileSystemStatus::kPermissionDenied),
                                /*bytes_written=*/0);
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::WriteStream(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    WriteStreamCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::WriteStreamImpl,
                     weak_factory_.GetWeakPtr(), offset, std::move(stream)),
      base::BindOnce([](WriteStreamCallback callback) {
        std::move(callback).Run(native_file_system_error::FromStatus(
                                    NativeFileSystemStatus::kPermissionDenied),
                                /*bytes_written=*/0);
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::Truncate(uint64_t length,
                                              TruncateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::TruncateImpl,
                     weak_factory_.GetWeakPtr(), length),
      base::BindOnce([](TruncateCallback callback) {
        std::move(callback).Run(native_file_system_error::FromStatus(
            NativeFileSystemStatus::kPermissionDenied));
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::Close(CloseCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::CloseImpl,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce([](CloseCallback callback) {
        std::move(callback).Run(native_file_system_error::FromStatus(
            NativeFileSystemStatus::kPermissionDenied));
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::WriteImpl(uint64_t offset,
                                               blink::mojom::BlobPtr data,
                                               WriteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_closed()) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kInvalidState,
            "An attempt was made to write to a closed writer."),
        /*bytes_written=*/0);
    return;
  }

  blob_context()->GetBlobDataFromBlobPtr(
      std::move(data),
      base::BindOnce(&NativeFileSystemFileWriterImpl::DoWriteBlob,
                     weak_factory_.GetWeakPtr(), std::move(callback), offset));
}

void NativeFileSystemFileWriterImpl::DoWriteBlob(
    WriteCallback callback,
    uint64_t position,
    std::unique_ptr<BlobDataHandle> blob) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!blob) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kInvalidArgument, "Blob does not exist"),
        /*bytes_written=*/0);
    return;
  }

  operation_runner()->Write(
      swap_url(), std::move(blob), position,
      base::BindRepeating(&NativeFileSystemFileWriterImpl::DidWrite,
                          weak_factory_.GetWeakPtr(),
                          base::Owned(new WriteState{std::move(callback)})));
}

void NativeFileSystemFileWriterImpl::WriteStreamImpl(
    uint64_t offset,
    mojo::ScopedDataPipeConsumerHandle stream,
    WriteStreamCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_closed()) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kInvalidState,
            "An attempt was made to write to a closed writer."),
        /*bytes_written=*/0);
    return;
  }

  operation_runner()->Write(
      swap_url(), std::move(stream), offset,
      base::BindRepeating(&NativeFileSystemFileWriterImpl::DidWrite,
                          weak_factory_.GetWeakPtr(),
                          base::Owned(new WriteState{std::move(callback)})));
}

void NativeFileSystemFileWriterImpl::DidWrite(WriteState* state,
                                              base::File::Error result,
                                              int64_t bytes,
                                              bool complete) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(state);
  state->bytes_written += bytes;
  if (complete) {
    std::move(state->callback)
        .Run(native_file_system_error::FromFileError(result),
             state->bytes_written);
  }
}

void NativeFileSystemFileWriterImpl::TruncateImpl(uint64_t length,
                                                  TruncateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (is_closed()) {
    std::move(callback).Run(native_file_system_error::FromStatus(
        NativeFileSystemStatus::kInvalidState,
        "An attempt was made to write to a closed writer."));
    return;
  }

  operation_runner()->Truncate(
      swap_url(), length,
      base::BindOnce(
          [](TruncateCallback callback, base::File::Error result) {
            std::move(callback).Run(
                native_file_system_error::FromFileError(result));
          },
          std::move(callback)));
}

void NativeFileSystemFileWriterImpl::CloseImpl(CloseCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);
  if (is_closed()) {
    std::move(callback).Run(native_file_system_error::FromStatus(
        NativeFileSystemStatus::kInvalidState,
        "An attempt was made to close an already closed writer."));
    return;
  }

  // Should the writer be destructed at this point, we want to allow the
  // close operation to run its course, so we should not purge the swap file.
  state_ = State::kClosePending;

  // If the move operation succeeds, the path pointing to the swap file
  // will not exist anymore.
  // In case of error, the swap file URL will point to a valid filesystem
  // location. The file at this URL will be deleted when the mojo pipe closes.
  // TODO(https://crbug.com/968556): Hook safebrowsing here, before the move.
  operation_runner()->Move(
      swap_url(), url(),
      storage::FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED,
      base::BindOnce(&NativeFileSystemFileWriterImpl::DidSwapFileBeforeClose,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NativeFileSystemFileWriterImpl::DidSwapFileBeforeClose(
    CloseCallback callback,
    base::File::Error result) {
  if (result != base::File::FILE_OK) {
    state_ = State::kCloseError;
    DLOG(ERROR) << "Swap file move operation failed source: "
                << swap_url().path() << " dest: " << url().path()
                << " error: " << base::File::ErrorToString(result);
  } else {
    state_ = State::kClosed;
  }

  std::move(callback).Run(native_file_system_error::FromFileError(result));
}

base::WeakPtr<NativeFileSystemHandleBase>
NativeFileSystemFileWriterImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
