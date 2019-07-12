// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_handle_base.h"

#include "base/task/post_task.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class NativeFileSystemHandleBase::UsageIndicatorTracker
    : public WebContentsObserver {
 public:
  UsageIndicatorTracker(int process_id,
                        int frame_id,
                        bool is_directory,
                        const base::FilePath& directory_path)
      : WebContentsObserver(
            WebContentsImpl::FromRenderFrameHostID(process_id, frame_id)),
        is_directory_(is_directory),
        directory_path_(directory_path) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (web_contents() && is_directory_)
      web_contents()->AddNativeFileSystemDirectoryHandle(directory_path_);
  }

  ~UsageIndicatorTracker() override {
    if (web_contents()) {
      if (is_directory_)
        web_contents()->RemoveNativeFileSystemDirectoryHandle(directory_path_);
      if (is_writable_)
        web_contents()->DecrementWritableNativeFileSystemHandleCount();
    }
  }

  void SetWritable(bool writable) {
    if (writable == is_writable_ || !web_contents())
      return;

    is_writable_ = writable;
    if (is_writable_)
      web_contents()->IncrementWritableNativeFileSystemHandleCount();
    else
      web_contents()->DecrementWritableNativeFileSystemHandleCount();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(WebContentsObserver::web_contents());
  }

 private:
  const bool is_directory_;
  const base::FilePath directory_path_;
  bool is_writable_ = false;
};

NativeFileSystemHandleBase::NativeFileSystemHandleBase(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state,
    bool is_directory)
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
  if (url_.type() == storage::kFileSystemTypeNativeLocal) {
    DCHECK_EQ(url_.mount_type(), storage::kFileSystemTypeIsolated);
    base::FilePath directory_path;
    if (is_directory) {
      // For usage reporting purposes try to get the root path of the isolated
      // file system, i.e. the path the user picked in a directory picker.
      auto* isolated_context = storage::IsolatedContext::GetInstance();
      if (!isolated_context->GetRegisteredPath(handle_state_.file_system.id(),
                                               &directory_path)) {
        // If for some reason the isolated file system no longer exists, fall
        // back to the path of the handle itself, which could be a child of the
        // originally picked path.
        directory_path = url.path();
      }
    }
    usage_indicator_tracker_ = base::SequenceBound<UsageIndicatorTracker>(
        base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::UI}),
        context_.process_id, context_.frame_id, bool{is_directory},
        base::FilePath(directory_path));
    UpdateWritableUsage();
  }
}

NativeFileSystemHandleBase::~NativeFileSystemHandleBase() = default;

NativeFileSystemHandleBase::PermissionStatus
NativeFileSystemHandleBase::GetReadPermissionStatus() {
  return handle_state_.read_grant->GetStatus();
}

NativeFileSystemHandleBase::PermissionStatus
NativeFileSystemHandleBase::GetWritePermissionStatus() {
  UpdateWritableUsage();
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

void NativeFileSystemHandleBase::UpdateWritableUsage() {
  if (!usage_indicator_tracker_)
    return;
  bool is_writable =
      handle_state_.read_grant->GetStatus() == PermissionStatus::GRANTED &&
      handle_state_.write_grant->GetStatus() == PermissionStatus::GRANTED;
  if (is_writable != was_writable_at_last_check_) {
    was_writable_at_last_check_ = is_writable;
    usage_indicator_tracker_.Post(
        FROM_HERE, &UsageIndicatorTracker::SetWritable, is_writable);
  }
}

}  // namespace content
