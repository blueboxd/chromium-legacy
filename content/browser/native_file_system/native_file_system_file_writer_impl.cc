// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_file_writer_impl.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"

using blink::mojom::NativeFileSystemError;

namespace content {

NativeFileSystemFileWriterImpl::NativeFileSystemFileWriterImpl(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : NativeFileSystemHandleBase(manager, context, url, handle_state) {}

NativeFileSystemFileWriterImpl::~NativeFileSystemFileWriterImpl() = default;

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
