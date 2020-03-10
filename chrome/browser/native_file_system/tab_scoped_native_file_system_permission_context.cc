// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/tab_scoped_native_file_system_permission_context.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/native_file_system/native_file_system_permission_context_factory.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/ui/native_file_system_dialogs.h"
#include "chrome/common/chrome_paths.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace {

using PermissionRequestOutcome =
    content::NativeFileSystemPermissionGrant::PermissionRequestOutcome;

void ShowWritePermissionPromptOnUIThread(
    int process_id,
    int frame_id,
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    base::OnceCallback<void(PermissionRequestOutcome outcome,
                            permissions::PermissionAction result)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(process_id, frame_id);
  if (!rfh || !rfh->IsCurrent()) {
    // Requested from a no longer valid render frame host.
    std::move(callback).Run(PermissionRequestOutcome::kInvalidFrame,
                            permissions::PermissionAction::DISMISSED);
    return;
  }

  if (!rfh->HasTransientUserActivation()) {
    // No permission prompts without user activation.
    std::move(callback).Run(PermissionRequestOutcome::kNoUserActivation,
                            permissions::PermissionAction::DISMISSED);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run(PermissionRequestOutcome::kInvalidFrame,
                            permissions::PermissionAction::DISMISSED);
    return;
  }

  url::Origin embedding_origin =
      url::Origin::Create(web_contents->GetLastCommittedURL());
  if (embedding_origin != origin) {
    // Third party iframes are not allowed to request more permissions.
    std::move(callback).Run(PermissionRequestOutcome::kThirdPartyContext,
                            permissions::PermissionAction::DISMISSED);
    return;
  }

  auto* request_manager =
      NativeFileSystemPermissionRequestManager::FromWebContents(web_contents);
  if (!request_manager) {
    std::move(callback).Run(PermissionRequestOutcome::kRequestAborted,
                            permissions::PermissionAction::DISMISSED);
    return;
  }

  // Drop fullscreen mode so that the user sees the URL bar.
  base::ScopedClosureRunner fullscreen_block =
      web_contents->ForSecurityDropFullscreen();

  request_manager->AddRequest(
      {origin, path, is_directory,
       NativeFileSystemPermissionRequestManager::Access::kWrite},
      base::BindOnce(
          [](base::OnceCallback<void(PermissionRequestOutcome outcome,
                                     permissions::PermissionAction result)>
                 callback,
             permissions::PermissionAction result) {
            switch (result) {
              case permissions::PermissionAction::GRANTED:
                std::move(callback).Run(PermissionRequestOutcome::kUserGranted,
                                        result);
                break;
              case permissions::PermissionAction::DENIED:
                std::move(callback).Run(PermissionRequestOutcome::kUserDenied,
                                        result);
                break;
              case permissions::PermissionAction::DISMISSED:
              case permissions::PermissionAction::IGNORED:
                std::move(callback).Run(
                    PermissionRequestOutcome::kUserDismissed, result);
                break;
              case permissions::PermissionAction::REVOKED:
              case permissions::PermissionAction::NUM:
                NOTREACHED();
                break;
            }
          },
          std::move(callback)),
      std::move(fullscreen_block));
}

// Returns a callback that calls the passed in |callback| by posting a task to
// the current sequenced task runner.
template <typename... ResultTypes>
base::OnceCallback<void(ResultTypes... results)>
BindResultCallbackToCurrentSequence(
    base::OnceCallback<void(ResultTypes... results)> callback) {
  return base::BindOnce(
      [](scoped_refptr<base::TaskRunner> task_runner,
         base::OnceCallback<void(ResultTypes... results)> callback,
         ResultTypes... results) {
        task_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), results...));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));
}

// Read permissions start out as granted, and can change to denied if the user
// revokes them for a tab. Re-requesting read permission is not currently
// supported.
class ReadPermissionGrantImpl
    : public content::NativeFileSystemPermissionGrant {
 public:
  ReadPermissionGrantImpl() = default;

  // NativeFileSystemPermissionGrant:
  PermissionStatus GetStatus() override { return status_; }
  void RequestPermission(
      int process_id,
      int frame_id,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override {
    // Requesting read permission is not currently supported, so just call the
    // callback right away.
    std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
  }

  void RevokePermission() {
    status_ = PermissionStatus::DENIED;
    NotifyPermissionStatusChanged();
  }

 protected:
  ~ReadPermissionGrantImpl() override = default;

 private:
  // This member should only be updated via RevokePermission(), to make sure
  // observers are properly notified about any change in status.
  PermissionStatus status_ = PermissionStatus::GRANTED;
};

}  // namespace

bool TabScopedNativeFileSystemPermissionContext::WritePermissionGrantImpl::Key::
operator==(const Key& rhs) const {
  return std::tie(process_id, frame_id, path) ==
         std::tie(rhs.process_id, rhs.frame_id, rhs.path);
}
bool TabScopedNativeFileSystemPermissionContext::WritePermissionGrantImpl::Key::
operator<(const Key& rhs) const {
  return std::tie(process_id, frame_id, path) <
         std::tie(rhs.process_id, rhs.frame_id, rhs.path);
}

TabScopedNativeFileSystemPermissionContext::WritePermissionGrantImpl::
    WritePermissionGrantImpl(
        base::WeakPtr<TabScopedNativeFileSystemPermissionContext> context,
        const url::Origin& origin,
        const Key& key,
        bool is_directory)
    : context_(std::move(context)),
      origin_(origin),
      key_(key),
      is_directory_(is_directory) {}

blink::mojom::PermissionStatus TabScopedNativeFileSystemPermissionContext::
    WritePermissionGrantImpl::GetStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return status_;
}

void TabScopedNativeFileSystemPermissionContext::WritePermissionGrantImpl::
    RequestPermission(
        int process_id,
        int frame_id,
        base::OnceCallback<void(PermissionRequestOutcome)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Check if a permission request has already been processed previously. This
  // check is done first because we don't want to reset the status of a write
  // permission if it has already been granted.
  if (GetStatus() != PermissionStatus::ASK) {
    std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
    return;
  }

  const ContentSetting content_setting =
      context_->GetWriteGuardContentSetting(origin_);
  switch (content_setting) {
    // Content setting grants write permission without asking.
    case CONTENT_SETTING_ALLOW:
      OnPermissionRequestComplete(
          std::move(callback),
          PermissionRequestOutcome::kGrantedByContentSetting,
          permissions::PermissionAction::GRANTED);
      break;
    // Content setting blocks write permission.
    case CONTENT_SETTING_BLOCK:
      OnPermissionRequestComplete(
          std::move(callback),
          PermissionRequestOutcome::kBlockedByContentSetting,
          permissions::PermissionAction::DENIED);
      break;
    // Ask the user for permission.
    case CONTENT_SETTING_ASK:
      base::PostTask(
          FROM_HERE, {content::BrowserThread::UI},
          base::BindOnce(
              &ShowWritePermissionPromptOnUIThread, process_id, frame_id,
              origin_, path(), is_directory_,
              BindResultCallbackToCurrentSequence(base::BindOnce(
                  &WritePermissionGrantImpl::OnPermissionRequestComplete, this,
                  std::move(callback)))));
      break;
    default:
      NOTREACHED();
  }
}

void TabScopedNativeFileSystemPermissionContext::WritePermissionGrantImpl::
    SetStatus(PermissionStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (status_ == status)
    return;
  status_ = status;
  NotifyPermissionStatusChanged();
}

TabScopedNativeFileSystemPermissionContext::WritePermissionGrantImpl::
    ~WritePermissionGrantImpl() {
  if (context_)
    context_->PermissionGrantDestroyed(this);
}

void TabScopedNativeFileSystemPermissionContext::WritePermissionGrantImpl::
    OnPermissionRequestComplete(
        base::OnceCallback<void(PermissionRequestOutcome)> callback,
        PermissionRequestOutcome outcome,
        permissions::PermissionAction result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (result) {
    case permissions::PermissionAction::GRANTED:
      SetStatus(PermissionStatus::GRANTED);
      break;
    case permissions::PermissionAction::DENIED:
      SetStatus(PermissionStatus::DENIED);
      break;
    case permissions::PermissionAction::DISMISSED:
    case permissions::PermissionAction::IGNORED:
      break;
    case permissions::PermissionAction::REVOKED:
    case permissions::PermissionAction::NUM:
      NOTREACHED();
      break;
  }

  base::UmaHistogramEnumeration(
      "NativeFileSystemAPI.WritePermissionRequestOutcome", outcome);
  if (is_directory_) {
    base::UmaHistogramEnumeration(
        "NativeFileSystemAPI.WritePermissionRequestOutcome.Directory", outcome);
  } else {
    base::UmaHistogramEnumeration(
        "NativeFileSystemAPI.WritePermissionRequestOutcome.File", outcome);
  }

  std::move(callback).Run(outcome);
}

struct TabScopedNativeFileSystemPermissionContext::OriginState {
  // Raw pointers, owned collectively by all the handles that reference this
  // grant. When last reference goes away this state is cleared as well by
  // PermissionGrantDestroyed().
  // TODO(mek): Lifetime of grants might change depending on the outcome of
  // the discussions surrounding lifetime of non-persistent permissions.
  std::map<WritePermissionGrantImpl::Key, WritePermissionGrantImpl*> grants;

  // Read permissions are keyed off of the tab they are associated with.
  // TODO(https://crbug.com/984769): This will change when permissions are no
  // longer scoped to individual tabs.
  using ReadPermissionKey = std::pair<int, int>;
  std::map<ReadPermissionKey, scoped_refptr<ReadPermissionGrantImpl>>
      directory_read_grants;
};

TabScopedNativeFileSystemPermissionContext::
    TabScopedNativeFileSystemPermissionContext(content::BrowserContext* context)
    : ChromeNativeFileSystemPermissionContext(context) {}

TabScopedNativeFileSystemPermissionContext::
    ~TabScopedNativeFileSystemPermissionContext() = default;

scoped_refptr<content::NativeFileSystemPermissionGrant>
TabScopedNativeFileSystemPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    int process_id,
    int frame_id,
    UserAction user_action) {
  if (!is_directory) {
    // No need to keep track of file read permissions, so just return a new
    // grant.
    return base::MakeRefCounted<ReadPermissionGrantImpl>();
  }

  // operator[] might insert a new OriginState in |origins_|, but that is
  // exactly what we want.
  auto& origin_state = origins_[origin];
  auto& existing_grant =
      origin_state.directory_read_grants[OriginState::ReadPermissionKey(
          process_id, frame_id)];
  if (!existing_grant)
    existing_grant = base::MakeRefCounted<ReadPermissionGrantImpl>();
  return existing_grant;
}

scoped_refptr<content::NativeFileSystemPermissionGrant>
TabScopedNativeFileSystemPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    int process_id,
    int frame_id,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |origins_|, but that is
  // exactly what we want.
  auto& origin_state = origins_[origin];
  // TODO(https://crbug.com/984772): If a parent directory is already writable
  // this newly returned grant should also be writable.
  // TODO(https://crbug.com/984769): Process ID and frame ID should not be used
  // to identify grants.
  WritePermissionGrantImpl::Key grant_key{path, process_id, frame_id};
  auto*& existing_grant = origin_state.grants[grant_key];

  ContentSetting content_setting = GetWriteGuardContentSetting(origin);

  if (existing_grant) {
    switch (content_setting) {
      case CONTENT_SETTING_ALLOW:
        existing_grant->SetStatus(
            WritePermissionGrantImpl::PermissionStatus::GRANTED);
        break;
      case CONTENT_SETTING_ASK:
        if (user_action == UserAction::kSave) {
          existing_grant->SetStatus(
              WritePermissionGrantImpl::PermissionStatus::GRANTED);
        }
        break;
      case CONTENT_SETTING_BLOCK:
        // We won't revoke permission to existing grants.
        break;
      default:
        NOTREACHED();
        break;
    }
    return existing_grant;
  }

  // If a grant does not exist for |origin|, create one, compute the permission
  // status, and store a reference to it in |origin_state| by assigning
  // |existing_grant|.
  auto result = base::MakeRefCounted<WritePermissionGrantImpl>(
      weak_factory_.GetWeakPtr(), origin, grant_key, is_directory);
  switch (content_setting) {
    case CONTENT_SETTING_ALLOW:
      result->SetStatus(WritePermissionGrantImpl::PermissionStatus::GRANTED);
      break;
    case CONTENT_SETTING_ASK:
      if (user_action == UserAction::kSave)
        result->SetStatus(WritePermissionGrantImpl::PermissionStatus::GRANTED);
      break;
    case CONTENT_SETTING_BLOCK:
      result->SetStatus(WritePermissionGrantImpl::PermissionStatus::DENIED);
      break;
    default:
      NOTREACHED();
  }
  existing_grant = result.get();
  return result;
}

ChromeNativeFileSystemPermissionContext::Grants
TabScopedNativeFileSystemPermissionContext::GetPermissionGrants(
    const url::Origin& origin,
    int process_id,
    int frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Grants grants;
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return grants;
  for (const auto& entry : it->second.grants) {
    if (entry.second->key().process_id != process_id ||
        entry.second->key().frame_id != frame_id) {
      continue;
    }
    if (entry.second->GetStatus() ==
        WritePermissionGrantImpl::PermissionStatus::GRANTED) {
      if (entry.second->is_directory()) {
        grants.directory_write_grants.push_back(entry.second->path());
      } else {
        grants.file_write_grants.push_back(entry.second->path());
      }
    }
  }
  return grants;
}

void TabScopedNativeFileSystemPermissionContext::RevokeDirectoryReadGrants(
    const url::Origin& origin,
    int process_id,
    int frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto origin_it = origins_.find(origin);
  if (origin_it == origins_.end()) {
    // No grants for origin, return directly.
    return;
  }
  OriginState& origin_state = origin_it->second;
  auto grant_it = origin_state.directory_read_grants.find(
      OriginState::ReadPermissionKey(process_id, frame_id));
  if (grant_it == origin_state.directory_read_grants.end()) {
    // Nothing to revoke.
    return;
  }
  // Revoke existing grant to stop existing handles from being able to read
  // directories.
  grant_it->second->RevokePermission();
  // And remove grant from map so future handles will get a new grant.
  origin_state.directory_read_grants.erase(grant_it);
}

void TabScopedNativeFileSystemPermissionContext::RevokeWriteGrants(
    const url::Origin& origin,
    int process_id,
    int frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto origin_it = origins_.find(origin);
  if (origin_it == origins_.end()) {
    // No grants for origin, return directly.
    return;
  }
  OriginState& origin_state = origin_it->second;
  // Grants are ordered first by process_id and frame_id, and only within that
  // by path. As a result lower_bound on a key with an empty path returns the
  // first grant to remove (if any), and all other grants to remove are
  // immediately after it.
  auto grant_it = origin_state.grants.lower_bound(
      WritePermissionGrantImpl::Key{base::FilePath(), process_id, frame_id});
  while (grant_it != origin_state.grants.end() &&
         grant_it->first.process_id == process_id &&
         grant_it->first.frame_id == frame_id) {
    grant_it->second->SetStatus(PermissionStatus::ASK);
    ++grant_it;
  }
}

void TabScopedNativeFileSystemPermissionContext::RevokeGrants(
    const url::Origin& origin,
    int process_id,
    int frame_id) {
  RevokeDirectoryReadGrants(origin, process_id, frame_id);
  RevokeWriteGrants(origin, process_id, frame_id);
}

void TabScopedNativeFileSystemPermissionContext::PermissionGrantDestroyed(
    WritePermissionGrantImpl* grant) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(grant->origin());
  DCHECK(it != origins_.end());
  it->second.grants.erase(grant->key());
}

base::WeakPtr<ChromeNativeFileSystemPermissionContext>
TabScopedNativeFileSystemPermissionContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
