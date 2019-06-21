// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_handle_base.h"

namespace content {

NativeFileSystemHandleBase::NativeFileSystemHandleBase(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : manager_(manager),
      context_(context),
      url_(url),
      handle_state_(handle_state) {
  DCHECK(manager_);
  DCHECK_EQ(url_.mount_type() == storage::kFileSystemTypeIsolated,
            handle_state_.file_system.is_valid())
      << url_.mount_type();
  // For now only support sandboxed file system and native file system.
  DCHECK(url_.type() == storage::kFileSystemTypeNativeLocal ||
         url_.type() == storage::kFileSystemTypePersistent ||
         url_.type() == storage::kFileSystemTypeTemporary ||
         url_.type() == storage::kFileSystemTypeTest)
      << url_.type();
}

NativeFileSystemHandleBase::~NativeFileSystemHandleBase() = default;

NativeFileSystemHandleBase::PermissionStatus
NativeFileSystemHandleBase::GetReadPermissionStatus() {
  return handle_state_.read_grant->GetStatus();
}

NativeFileSystemHandleBase::PermissionStatus
NativeFileSystemHandleBase::GetWritePermissionStatus() {
  // It is not currently possible to have write only handles, so first check the
  // read permission status. See also:
  // http://wicg.github.io/native-file-system/#api-filesystemhandle-querypermission
  PermissionStatus read_status = GetReadPermissionStatus();
  if (read_status != PermissionStatus::GRANTED)
    return read_status;

  return handle_state_.write_grant->GetStatus();
}

void NativeFileSystemHandleBase::DoGetPermissionStatus(
    bool writable,
    base::OnceCallback<void(PermissionStatus)> callback) {
  std::move(callback).Run(writable ? GetWritePermissionStatus()
                                   : GetReadPermissionStatus());
}

void NativeFileSystemHandleBase::DoRequestPermission(
    bool writable,
    base::OnceCallback<void(PermissionStatus)> callback) {
  PermissionStatus current_status =
      writable ? GetWritePermissionStatus() : GetReadPermissionStatus();
  if (current_status != PermissionStatus::ASK) {
    std::move(callback).Run(current_status);
    return;
  }
  if (!writable) {
    handle_state_.read_grant->RequestPermission(
        context().process_id, context().frame_id,
        base::BindOnce(&NativeFileSystemHandleBase::DoGetPermissionStatus,
                       AsWeakPtr(), writable, std::move(callback)));
    return;
  }

  // TODO(https://crbug.com/971401): Today read permission isn't revokable, so
  // current status should always be GRANTED.
  DCHECK_EQ(GetReadPermissionStatus(), PermissionStatus::GRANTED);

  handle_state_.write_grant->RequestPermission(
      context().process_id, context().frame_id,
      base::BindOnce(&NativeFileSystemHandleBase::DoGetPermissionStatus,
                     AsWeakPtr(), writable, std::move(callback)));
}

}  // namespace content
