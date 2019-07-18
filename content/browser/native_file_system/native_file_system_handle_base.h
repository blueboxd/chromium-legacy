// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/common/content_export.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/fileapi/isolated_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace storage {
class FileSystemContext;
class FileSystemOperationRunner;
class BlobStorageContext;
}  // namespace storage

namespace content {

// Base class for File and Directory handle implementations. Holds data that is
// common to both and (will) deal with functionality that is common as well,
// such as permission requests. Instances of this class should be owned by the
// NativeFileSystemManagerImpl instance passed in to the constructor.
//
// This class is not thread safe, all methods should only be called on the IO
// thread. This is because code interacts directly with the file system backends
// (via storage::FileSystemContext and store::FileSystemOperationRunner, which
// both expect some of their methods to only be called on the IO thread).
class CONTENT_EXPORT NativeFileSystemHandleBase
    : public NativeFileSystemPermissionGrant::Observer {
 public:
  using BindingContext = NativeFileSystemManagerImpl::BindingContext;
  using SharedHandleState = NativeFileSystemManagerImpl::SharedHandleState;
  using PermissionStatus = blink::mojom::PermissionStatus;

  NativeFileSystemHandleBase(NativeFileSystemManagerImpl* manager,
                             const BindingContext& context,
                             const storage::FileSystemURL& url,
                             const SharedHandleState& handle_state,
                             bool is_directory);
  ~NativeFileSystemHandleBase() override;

  const storage::FileSystemURL& url() const { return url_; }
  const SharedHandleState& handle_state() const { return handle_state_; }
  const storage::IsolatedContext::ScopedFSHandle& file_system() const {
    return handle_state_.file_system;
  }

  PermissionStatus GetReadPermissionStatus();
  PermissionStatus GetWritePermissionStatus();

  // Implementation for the GetPermissionStatus method in the
  // blink::mojom::NativeFileSystemFileHandle and DirectoryHandle interfaces.
  void DoGetPermissionStatus(
      bool writable,
      base::OnceCallback<void(PermissionStatus)> callback);
  // Implementation for the RequestPermission method in the
  // blink::mojom::NativeFileSystemFileHandle and DirectoryHandle interfaces.
  void DoRequestPermission(bool writable,
                           base::OnceCallback<void(PermissionStatus)> callback);

  // Invokes |callback|, possibly after first requesting write permission. If
  // permission isn't granted, |permission_denied| is invoked instead. The
  // callbacks can be invoked synchronously.
  template <typename CallbackArgType>
  void RunWithWritePermission(
      base::OnceCallback<void(CallbackArgType)> callback,
      base::OnceCallback<void(CallbackArgType)> no_permission_callback,
      CallbackArgType callback_arg);

 protected:
  NativeFileSystemManagerImpl* manager() { return manager_; }
  const BindingContext& context() { return context_; }
  storage::FileSystemOperationRunner* operation_runner() {
    return manager()->operation_runner();
  }
  storage::FileSystemContext* file_system_context() {
    return manager()->context();
  }
  storage::BlobStorageContext* blob_context() {
    return manager()->blob_context();
  }

  virtual base::WeakPtr<NativeFileSystemHandleBase> AsWeakPtr() = 0;

  // NativeFileSystemPermissionGrant::Observer:
  void OnPermissionStatusChanged() override;

 private:
  // The NativeFileSystemManagerImpl that owns this instance.
  NativeFileSystemManagerImpl* const manager_;
  const BindingContext context_;
  const storage::FileSystemURL url_;
  const SharedHandleState handle_state_;

  class UsageIndicatorTracker;
  base::SequenceBound<UsageIndicatorTracker> usage_indicator_tracker_;
  bool was_writable_at_last_check_ = false;
  bool was_readable_at_last_check_ = true;

  void UpdateUsage();

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemHandleBase);
};

template <typename CallbackArgType>
void NativeFileSystemHandleBase::RunWithWritePermission(
    base::OnceCallback<void(CallbackArgType)> callback,
    base::OnceCallback<void(CallbackArgType)> no_permission_callback,
    CallbackArgType callback_arg) {
  DoRequestPermission(
      /*writable=*/true,
      base::BindOnce(
          [](base::OnceCallback<void(CallbackArgType)> callback,
             base::OnceCallback<void(CallbackArgType)> no_permission_callback,
             CallbackArgType callback_arg,
             blink::mojom::PermissionStatus status) {
            if (status == blink::mojom::PermissionStatus::GRANTED) {
              std::move(callback).Run(std::move(callback_arg));
              return;
            }
            std::move(no_permission_callback).Run(std::move(callback_arg));
          },
          std::move(callback), std::move(no_permission_callback),
          std::move(callback_arg)));
}

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_
