// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_file_writer_impl.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"

using blink::mojom::NativeFileSystemError;
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
    const SharedHandleState& handle_state)
    : NativeFileSystemHandleBase(manager,
                                 context,
                                 url,
                                 handle_state,
                                 /*is_directory=*/false) {}

NativeFileSystemFileWriterImpl::~NativeFileSystemFileWriterImpl() = default;

void NativeFileSystemFileWriterImpl::Write(uint64_t offset,
                                           blink::mojom::BlobPtr data,
                                           WriteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::WriteImpl,
                     weak_factory_.GetWeakPtr(), offset, std::move(data)),
      base::BindOnce([](WriteCallback callback) {
        std::move(callback).Run(
            NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED),
            0);
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
        std::move(callback).Run(
            NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED),
            0);
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
        std::move(callback).Run(
            NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED));
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::Close(CloseCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileWriterImpl::CloseImpl,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce([](CloseCallback callback) {
        std::move(callback).Run(
            NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED));
      }),
      std::move(callback));
}

void NativeFileSystemFileWriterImpl::WriteImpl(uint64_t offset,
                                               blink::mojom::BlobPtr data,
                                               WriteCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

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
        NativeFileSystemError::New(base::File::FILE_ERROR_FAILED), 0);
    return;
  }

  operation_runner()->Write(
      url(), std::move(blob), position,
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

  operation_runner()->Write(
      url(), std::move(stream), offset,
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
        .Run(NativeFileSystemError::New(result), state->bytes_written);
  }
}

void NativeFileSystemFileWriterImpl::TruncateImpl(uint64_t length,
                                                  TruncateCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  operation_runner()->Truncate(
      url(), length,
      base::BindOnce(
          [](TruncateCallback callback, base::File::Error result) {
            std::move(callback).Run(NativeFileSystemError::New(result));
          },
          std::move(callback)));
}

void NativeFileSystemFileWriterImpl::CloseImpl(CloseCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);
  std::move(callback).Run(NativeFileSystemError::New(base::File::FILE_OK));
}

base::WeakPtr<NativeFileSystemHandleBase>
NativeFileSystemFileWriterImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
