// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"

#include "base/guid.h"
#include "net/base/mime_util.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom.h"

using blink::mojom::NativeFileSystemError;
using storage::BlobDataHandle;
using storage::BlobImpl;
using storage::FileSystemOperation;

namespace content {

NativeFileSystemFileHandleImpl::NativeFileSystemFileHandleImpl(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : NativeFileSystemHandleBase(manager,
                                 context,
                                 url,
                                 handle_state,
                                 /*is_directory=*/false) {}

NativeFileSystemFileHandleImpl::~NativeFileSystemFileHandleImpl() = default;

void NativeFileSystemFileHandleImpl::GetPermissionStatus(
    bool writable,
    GetPermissionStatusCallback callback) {
  DoGetPermissionStatus(writable, std::move(callback));
}

void NativeFileSystemFileHandleImpl::RequestPermission(
    bool writable,
    RequestPermissionCallback callback) {
  DoRequestPermission(writable, std::move(callback));
}

void NativeFileSystemFileHandleImpl::AsBlob(AsBlobCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(
        NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED),
        nullptr);
    return;
  }

  // TODO(mek): Check backend::SupportsStreaming and create snapshot file if
  // streaming is not supported.
  operation_runner()->GetMetadata(
      url(),
      FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          FileSystemOperation::GET_METADATA_FIELD_SIZE |
          FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::BindOnce(&NativeFileSystemFileHandleImpl::DidGetMetaDataForBlob,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NativeFileSystemFileHandleImpl::Remove(RemoveCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileHandleImpl::RemoveImpl,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce([](RemoveCallback callback) {
        std::move(callback).Run(
            NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED));
      }),
      std::move(callback));
}

void NativeFileSystemFileHandleImpl::CreateFileWriter(
    CreateFileWriterCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileHandleImpl::CreateFileWriterImpl,
                     weak_factory_.GetWeakPtr()),
      base::BindOnce([](CreateFileWriterCallback callback) {
        std::move(callback).Run(
            NativeFileSystemError::New(base::File::FILE_ERROR_ACCESS_DENIED),
            nullptr);
      }),
      std::move(callback));
}

void NativeFileSystemFileHandleImpl::Transfer(
    blink::mojom::NativeFileSystemTransferTokenRequest token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  manager()->CreateTransferToken(*this, std::move(token));
}

void NativeFileSystemFileHandleImpl::DidGetMetaDataForBlob(
    AsBlobCallback callback,
    base::File::Error result,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(NativeFileSystemError::New(result), nullptr);
    return;
  }

  std::string uuid = base::GenerateGUID();
  auto blob_builder = std::make_unique<storage::BlobDataBuilder>(uuid);
  // Use AppendFileSystemFile here, since we're streaming the file directly
  // from the file system backend, and the file thus might not actually be
  // backed by a file on disk.
  blob_builder->AppendFileSystemFile(url().ToGURL(), 0, -1, info.last_modified,
                                     file_system_context());

  base::FilePath::StringType extension = url().path().Extension();
  if (!extension.empty()) {
    std::string mime_type;
    // TODO(https://crbug.com/962306): Using GetMimeTypeFromExtension and
    // including platform defined mime type mappings might be nice/make sense,
    // however that method can potentially block and thus can't be called from
    // the IO thread.
    if (net::GetWellKnownMimeTypeFromExtension(extension.substr(1), &mime_type))
      blob_builder->set_content_type(mime_type);
  }

  std::unique_ptr<BlobDataHandle> blob_handle =
      blob_context()->AddFinishedBlob(std::move(blob_builder));
  if (blob_handle->IsBroken()) {
    std::move(callback).Run(
        NativeFileSystemError::New(base::File::FILE_ERROR_FAILED), nullptr);
    return;
  }

  std::string content_type = blob_handle->content_type();
  blink::mojom::BlobPtr blob_ptr;
  BlobImpl::Create(std::move(blob_handle), mojo::MakeRequest(&blob_ptr));

  std::move(callback).Run(
      NativeFileSystemError::New(base::File::FILE_OK),
      blink::mojom::SerializedBlob::New(uuid, content_type, info.size,
                                        blob_ptr.PassInterface()));
}

void NativeFileSystemFileHandleImpl::RemoveImpl(RemoveCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  operation_runner()->RemoveFile(
      url(), base::BindOnce(
                 [](RemoveCallback callback, base::File::Error result) {
                   std::move(callback).Run(NativeFileSystemError::New(result));
                 },
                 std::move(callback)));
}

void NativeFileSystemFileHandleImpl::CreateFileWriterImpl(
    CreateFileWriterCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  std::move(callback).Run(
      NativeFileSystemError::New(base::File::FILE_OK),
      manager()->CreateFileWriter(context(), url(), handle_state()));
}

base::WeakPtr<NativeFileSystemHandleBase>
NativeFileSystemFileHandleImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content
