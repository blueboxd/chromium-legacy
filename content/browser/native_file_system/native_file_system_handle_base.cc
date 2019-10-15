// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_handle_base.h"

#include "base/task/post_task.h"
#include "content/browser/native_file_system/native_file_system_error.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
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
    // Disable back-forward cache as native file system's usage of
    // RenderFrameHost::IsCurrent at the moment is not compatible with bfcache.
    BackForwardCache::DisableForRenderFrameHost(
        GlobalFrameRoutingId(process_id, frame_id), "NativeFileSystem");
    if (web_contents()) {
      web_contents()->IncrementNativeFileSystemHandleCount();
      if (is_directory_)
        web_contents()->AddNativeFileSystemDirectoryHandle(directory_path_);
    }
  }

  ~UsageIndicatorTracker() override {
    if (web_contents()) {
      web_contents()->DecrementNativeFileSystemHandleCount();
      if (is_directory_ && is_readable_)
        web_contents()->RemoveNativeFileSystemDirectoryHandle(directory_path_);
      if (is_writable_)
        web_contents()->DecrementWritableNativeFileSystemHandleCount();
    }
  }

  void UpdateStatus(bool readable, bool writable) {
    SetReadable(readable);
    SetWritable(writable);
  }

  void SetReadable(bool readable) {
    if (readable == is_readable_ || !web_contents())
      return;

    is_readable_ = readable;
    if (is_directory_) {
      if (is_readable_)
        web_contents()->AddNativeFileSystemDirectoryHandle(directory_path_);
      else
        web_contents()->RemoveNativeFileSystemDirectoryHandle(directory_path_);
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
  bool is_readable_ = true;
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

    handle_state_.read_grant->AddObserver(this);
    // In some cases we use the same grant for read and write access. In that
    // case only add an observer once.
    if (handle_state_.read_grant != handle_state_.write_grant)
      handle_state_.write_grant->AddObserver(this);

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
        base::CreateSingleThreadTaskRunner({BrowserThread::UI}),
        context_.process_id, context_.frame_id, bool{is_directory},
        base::FilePath(directory_path));
    UpdateUsage();
  }
}

NativeFileSystemHandleBase::~NativeFileSystemHandleBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It is fine to remove an observer that never was added, so no need to check
  // for URL type and/or the same grant being used for read and write access.
  handle_state_.read_grant->RemoveObserver(this);
  handle_state_.write_grant->RemoveObserver(this);
}

NativeFileSystemHandleBase::PermissionStatus
NativeFileSystemHandleBase::GetReadPermissionStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return handle_state_.read_grant->GetStatus();
}

NativeFileSystemHandleBase::PermissionStatus
NativeFileSystemHandleBase::GetWritePermissionStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateUsage();
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(writable ? GetWritePermissionStatus()
                                   : GetReadPermissionStatus());
}

void NativeFileSystemHandleBase::DoRequestPermission(
    bool writable,
    base::OnceCallback<void(blink::mojom::NativeFileSystemErrorPtr,
                            PermissionStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PermissionStatus current_status =
      writable ? GetWritePermissionStatus() : GetReadPermissionStatus();
  // If we already have a valid permission status, just return that. Also just
  // return the current permission status if this is called from a worker, as we
  // don't support prompting for increased permissions from workers.
  //
  // Currently the worker check here is redundant because there is no way for
  // workers to get native file system handles. While workers will never be able
  // to call chooseEntries(), they will be able to receive existing handles from
  // windows via postMessage() and IndexedDB.
  if (current_status != PermissionStatus::ASK || context_.is_worker()) {
    std::move(callback).Run(native_file_system_error::Ok(), current_status);
    return;
  }
  if (!writable) {
    handle_state_.read_grant->RequestPermission(
        context().process_id, context().frame_id,
        base::BindOnce(&NativeFileSystemHandleBase::DidRequestPermission,
                       AsWeakPtr(), writable, std::move(callback)));
    return;
  }

  // TODO(https://crbug.com/971401): Today we can't prompt for read permission,
  // and should never be in state "ASK". Since we already checked for DENIED
  // above current status here should always be GRANTED.
  DCHECK_EQ(GetReadPermissionStatus(), PermissionStatus::GRANTED);

  handle_state_.write_grant->RequestPermission(
      context().process_id, context().frame_id,
      base::BindOnce(&NativeFileSystemHandleBase::DidRequestPermission,
                     AsWeakPtr(), writable, std::move(callback)));
}

void NativeFileSystemHandleBase::DidRequestPermission(
    bool writable,
    base::OnceCallback<void(blink::mojom::NativeFileSystemErrorPtr,
                            PermissionStatus)> callback,
    NativeFileSystemPermissionGrant::PermissionRequestOutcome outcome) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  using Outcome = NativeFileSystemPermissionGrant::PermissionRequestOutcome;
  switch (outcome) {
    case Outcome::kInvalidFrame:
    case Outcome::kThirdPartyContext:
      std::move(callback).Run(
          native_file_system_error::FromStatus(
              blink::mojom::NativeFileSystemStatus::kPermissionDenied,
              "Not allowed to request permissions in this context."),
          writable ? GetWritePermissionStatus() : GetReadPermissionStatus());
      break;
    case Outcome::kNoUserActivation:
      std::move(callback).Run(
          native_file_system_error::FromStatus(
              blink::mojom::NativeFileSystemStatus::kPermissionDenied,
              "User activation is required to request permissions."),
          writable ? GetWritePermissionStatus() : GetReadPermissionStatus());
      break;
    case Outcome::kBlockedByContentSetting:
    case Outcome::kUserGranted:
    case Outcome::kUserDenied:
    case Outcome::kUserDismissed:
    case Outcome::kRequestAborted:
      std::move(callback).Run(
          native_file_system_error::Ok(),
          writable ? GetWritePermissionStatus() : GetReadPermissionStatus());
      return;
  }
  NOTREACHED();
}

void NativeFileSystemHandleBase::UpdateUsage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!usage_indicator_tracker_)
    return;
  bool is_readable =
      handle_state_.read_grant->GetStatus() == PermissionStatus::GRANTED;
  bool is_writable = is_readable && handle_state_.write_grant->GetStatus() ==
                                        PermissionStatus::GRANTED;
  if (is_writable != was_writable_at_last_check_ ||
      is_readable != was_readable_at_last_check_) {
    was_readable_at_last_check_ = is_readable;
    was_writable_at_last_check_ = is_writable;
    usage_indicator_tracker_.Post(FROM_HERE,
                                  &UsageIndicatorTracker::UpdateStatus,
                                  is_readable, is_writable);
  }
}

void NativeFileSystemHandleBase::OnPermissionStatusChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateUsage();
}

}  // namespace content
