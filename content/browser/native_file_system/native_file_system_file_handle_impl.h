// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_IMPL_H_

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/native_file_system/native_file_system_handle_base.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/common/content_export.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_file_handle.mojom.h"

namespace content {

// This is the browser side implementation of the
// NativeFileSystemFileHandle mojom interface. Instances of this class are
// owned by the NativeFileSystemManagerImpl instance passed in to the
// constructor.
//
// This class is not thread safe, all methods should be called on the IO thread.
// The link to the IO thread is due to its dependencies on both the blob system
// (via storage::BlobStorageContext) and the file system backends (via
// storage::FileSystemContext and storage::FileSystemOperationRunner, which both
// expect some of their methods to always be called on the IO thread).
// See https://crbug.com/957249 for some thoughts about the blob system aspect
// of this.
class CONTENT_EXPORT NativeFileSystemFileHandleImpl
    : public NativeFileSystemHandleBase,
      public blink::mojom::NativeFileSystemFileHandle {
 public:
  NativeFileSystemFileHandleImpl(NativeFileSystemManagerImpl* manager,
                                 const BindingContext& context,
                                 const storage::FileSystemURL& url,
                                 const SharedHandleState& handle_state);
  ~NativeFileSystemFileHandleImpl() override;

  // blink::mojom::NativeFileSystemFileHandle:
  void GetPermissionStatus(bool writable,
                           GetPermissionStatusCallback callback) override;
  void RequestPermission(bool writable,
                         RequestPermissionCallback callback) override;
  void AsBlob(AsBlobCallback callback) override;
  void Remove(RemoveCallback callback) override;
  void CreateFileWriter(CreateFileWriterCallback callback) override;
  void Transfer(
      blink::mojom::NativeFileSystemTransferTokenRequest token) override;

 private:
  void DidGetMetaDataForBlob(AsBlobCallback callback,
                             base::File::Error result,
                             const base::File::Info& info);

  void RemoveImpl(RemoveCallback callback);
  void CreateFileWriterImpl(CreateFileWriterCallback callback);

  base::WeakPtr<NativeFileSystemHandleBase> AsWeakPtr() override;

  base::WeakPtrFactory<NativeFileSystemFileHandleImpl> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemFileHandleImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_FILE_HANDLE_IMPL_H_
