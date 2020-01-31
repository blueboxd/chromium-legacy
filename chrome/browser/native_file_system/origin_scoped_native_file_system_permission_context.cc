// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/origin_scoped_native_file_system_permission_context.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_dialogs.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#endif

namespace {
using permissions::PermissionAction;

enum class GrantType { kRead, kWrite };
}  // namespace

class OriginScopedNativeFileSystemPermissionContext::PermissionGrantImpl
    : public content::NativeFileSystemPermissionGrant {
 public:
  PermissionGrantImpl(
      base::WeakPtr<OriginScopedNativeFileSystemPermissionContext> context,
      const url::Origin& origin,
      const base::FilePath& path,
      bool is_directory,
      GrantType type)
      : context_(std::move(context)),
        origin_(origin),
        path_(path),
        is_directory_(is_directory),
        type_(type) {}

  // NativeFileSystemPermissionGrant:
  PermissionStatus GetStatus() override { return status_; }
  void RequestPermission(
      int process_id,
      int frame_id,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) override {
    RequestPermissionImpl(process_id, frame_id,
                          /*require_user_gesture=*/true, std::move(callback));
  }

  void RequestPermissionImpl(
      int process_id,
      int frame_id,
      bool require_user_gesture,
      base::OnceCallback<void(PermissionRequestOutcome)> callback) {
    // Check if a permission request has already been processed previously. This
    // check is done first because we don't want to reset the status of a
    // permission if it has already been granted.
    if (GetStatus() != PermissionStatus::ASK || !context_) {
      std::move(callback).Run(PermissionRequestOutcome::kRequestAborted);
      return;
    }

    // Check if prompting for write access is blocked by the user and update
    // the status if it is.
    if (type_ == GrantType::kWrite &&
        !context_->CanRequestWritePermission(origin_)) {
      SetStatus(PermissionStatus::DENIED);
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback),
          PermissionRequestOutcome::kBlockedByContentSetting);
      return;
    }

    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(process_id, frame_id);
    if (!rfh || !rfh->IsCurrent()) {
      // Requested from a no longer valid render frame host.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kInvalidFrame);
      return;
    }

    if (require_user_gesture && !rfh->HasTransientUserActivation()) {
      // No permission prompts without user activation.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kNoUserActivation);
      return;
    }

    content::WebContents* web_contents =
        content::WebContents::FromRenderFrameHost(rfh);
    if (!web_contents) {
      // Requested from a worker, or a no longer existing tab.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kInvalidFrame);
      return;
    }

    url::Origin embedding_origin =
        url::Origin::Create(web_contents->GetLastCommittedURL());
    if (embedding_origin != origin_) {
      // Third party iframes are not allowed to request more permissions.
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kThirdPartyContext);
      return;
    }

    auto* request_manager =
        NativeFileSystemPermissionRequestManager::FromWebContents(web_contents);
    if (!request_manager) {
      RunCallbackAndRecordPermissionRequestOutcome(
          std::move(callback), PermissionRequestOutcome::kRequestAborted);
      return;
    }

    // Drop fullscreen mode so that the user sees the URL bar.
    web_contents->ForSecurityDropFullscreen();

    if (type_ == GrantType::kRead) {
      if (!is_directory_) {
        // TODO(mek): Implement requesting read permissions for files.
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kRequestAborted);
        return;
      }

      // TODO(mek): Handle directory read access prompting in RequestManager.
      ShowNativeFileSystemDirectoryAccessConfirmationDialog(
          origin_, path_,
          base::BindOnce(&PermissionGrantImpl::OnPermissionRequestResult, this,
                         std::move(callback)),
          web_contents);

      return;
    }

    request_manager->AddRequest(
        {origin_, path_, is_directory_},
        base::BindOnce(&PermissionGrantImpl::OnPermissionRequestResult, this,
                       std::move(callback)));
  }

  const url::Origin& origin() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return origin_;
  }

  bool is_directory() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return is_directory_;
  }

  const base::FilePath& path() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return path_;
  }

  GrantType type() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return type_;
  }

  void SetStatus(PermissionStatus status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (status_ == status)
      return;
    status_ = status;
    NotifyPermissionStatusChanged();
  }

  static void CollectGrants(
      const std::map<base::FilePath, PermissionGrantImpl*>& grants,
      std::vector<base::FilePath>* directory_grants,
      std::vector<base::FilePath>* file_grants) {
    for (const auto& entry : grants) {
      if (entry.second->GetStatus() != PermissionStatus::GRANTED)
        continue;
      if (entry.second->is_directory()) {
        directory_grants->push_back(entry.second->path());
      } else {
        file_grants->push_back(entry.second->path());
      }
    }
  }

 protected:
  ~PermissionGrantImpl() override {
    if (context_)
      context_->PermissionGrantDestroyed(this);
  }

 private:
  void OnPermissionRequestResult(
      base::OnceCallback<void(PermissionRequestOutcome)> callback,
      PermissionAction result) {
    switch (result) {
      case PermissionAction::GRANTED:
        SetStatus(PermissionStatus::GRANTED);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserGranted);
        if (context_)
          context_->ScheduleUsageIconUpdate();
        break;
      case PermissionAction::DENIED:
        SetStatus(PermissionStatus::DENIED);
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDenied);
        break;
      case PermissionAction::DISMISSED:
      case PermissionAction::IGNORED:
        RunCallbackAndRecordPermissionRequestOutcome(
            std::move(callback), PermissionRequestOutcome::kUserDismissed);
        break;
      case PermissionAction::REVOKED:
      case PermissionAction::NUM:
        NOTREACHED();
        break;
    }
  }

  void RunCallbackAndRecordPermissionRequestOutcome(
      base::OnceCallback<void(PermissionRequestOutcome)> callback,
      PermissionRequestOutcome outcome) {
    if (type_ == GrantType::kWrite) {
      base::UmaHistogramEnumeration(
          "NativeFileSystemAPI.WritePermissionRequestOutcome", outcome);
      if (is_directory_) {
        base::UmaHistogramEnumeration(
            "NativeFileSystemAPI.WritePermissionRequestOutcome.Directory",
            outcome);
      } else {
        base::UmaHistogramEnumeration(
            "NativeFileSystemAPI.WritePermissionRequestOutcome.File", outcome);
      }
    } else {
      base::UmaHistogramEnumeration(
          "NativeFileSystemAPI.ReadPermissionRequestOutcome", outcome);
      if (is_directory_) {
        base::UmaHistogramEnumeration(
            "NativeFileSystemAPI.ReadPermissionRequestOutcome.Directory",
            outcome);
      } else {
        base::UmaHistogramEnumeration(
            "NativeFileSystemAPI.ReadPermissionRequestOutcome.File", outcome);
      }
    }

    std::move(callback).Run(outcome);
  }

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtr<OriginScopedNativeFileSystemPermissionContext> const context_;
  const url::Origin origin_;
  const base::FilePath path_;
  const bool is_directory_;
  const GrantType type_;

  // This member should only be updated via SetStatus(), to make sure
  // observers are properly notified about any change in status.
  PermissionStatus status_ = PermissionStatus::ASK;
};

struct OriginScopedNativeFileSystemPermissionContext::OriginState {
  // Raw pointers, owned collectively by all the handles that reference this
  // grant. When last reference goes away this state is cleared as well by
  // PermissionGrantDestroyed().
  // TODO(mek): Revoke all permissions after the last tab for an origin gets
  // closed.
  std::map<base::FilePath, PermissionGrantImpl*> read_grants;
  std::map<base::FilePath, PermissionGrantImpl*> write_grants;
};

OriginScopedNativeFileSystemPermissionContext::
    OriginScopedNativeFileSystemPermissionContext(
        content::BrowserContext* context)
    : ChromeNativeFileSystemPermissionContext(context), profile_(context) {}

OriginScopedNativeFileSystemPermissionContext::
    ~OriginScopedNativeFileSystemPermissionContext() = default;

scoped_refptr<content::NativeFileSystemPermissionGrant>
OriginScopedNativeFileSystemPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    int process_id,
    int frame_id,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |origins_|, but that
  // is exactly what we want.
  auto& origin_state = origins_[origin];
  // TODO(https://crbug.com/984772): If a parent directory is already
  // readable this newly returned grant should also be readable.
  auto*& existing_grant = origin_state.read_grants[path];
  scoped_refptr<PermissionGrantImpl> new_grant;
  if (!existing_grant || existing_grant->is_directory() != is_directory) {
    new_grant = base::MakeRefCounted<PermissionGrantImpl>(
        weak_factory_.GetWeakPtr(), origin, path, is_directory,
        GrantType::kRead);
    existing_grant = new_grant.get();
  }

  // Files automatically get read access when picked by the user, directories
  // need to first be confirmed.
  if (user_action != UserAction::kLoadFromStorage && !is_directory) {
    existing_grant->SetStatus(PermissionStatus::GRANTED);
    ScheduleUsageIconUpdate();
  }

  return existing_grant;
}

scoped_refptr<content::NativeFileSystemPermissionGrant>
OriginScopedNativeFileSystemPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    int process_id,
    int frame_id,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |origins_|, but that
  // is exactly what we want.
  auto& origin_state = origins_[origin];
  // TODO(https://crbug.com/984772): If a parent directory is already
  // writable this newly returned grant should also be writable.
  auto*& existing_grant = origin_state.write_grants[path];
  scoped_refptr<PermissionGrantImpl> new_grant;
  if (!existing_grant || existing_grant->is_directory() != is_directory) {
    new_grant = base::MakeRefCounted<PermissionGrantImpl>(
        weak_factory_.GetWeakPtr(), origin, path, is_directory,
        GrantType::kWrite);
    existing_grant = new_grant.get();
  }

  if (CanRequestWritePermission(origin)) {
    if (user_action == UserAction::kSave) {
      existing_grant->SetStatus(PermissionStatus::GRANTED);
      ScheduleUsageIconUpdate();
    }
  } else if (new_grant) {
    existing_grant->SetStatus(PermissionStatus::DENIED);
  }

  return existing_grant;
}

void OriginScopedNativeFileSystemPermissionContext::ConfirmDirectoryReadAccess(
    const url::Origin& origin,
    const base::FilePath& path,
    int process_id,
    int frame_id,
    base::OnceCallback<void(PermissionStatus)> callback) {
  // TODO(mek): Once tab-scoped permission model is no longer used we can
  // refactor the calling code of this method to just do what this
  // implementation does directly.
  scoped_refptr<content::NativeFileSystemPermissionGrant> grant =
      GetReadPermissionGrant(origin, path, /*is_directory=*/true, process_id,
                             frame_id, UserAction::kOpen);
  static_cast<PermissionGrantImpl*>(grant.get())
      ->RequestPermissionImpl(
          process_id, frame_id, /*require_user_gesture=*/false,
          base::BindOnce(
              [](base::OnceCallback<void(PermissionStatus)> callback,
                 scoped_refptr<content::NativeFileSystemPermissionGrant> grant,
                 content::NativeFileSystemPermissionGrant::
                     PermissionRequestOutcome outcome) {
                std::move(callback).Run(grant->GetStatus());
              },
              std::move(callback), grant));
}

ChromeNativeFileSystemPermissionContext::Grants
OriginScopedNativeFileSystemPermissionContext::GetPermissionGrants(
    const url::Origin& origin,
    int process_id,
    int frame_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return {};

  Grants grants;
  PermissionGrantImpl::CollectGrants(it->second.read_grants,
                                     &grants.directory_read_grants,
                                     &grants.file_read_grants);
  PermissionGrantImpl::CollectGrants(it->second.write_grants,
                                     &grants.directory_write_grants,
                                     &grants.file_write_grants);
  return grants;
}

void OriginScopedNativeFileSystemPermissionContext::RevokeGrants(
    const url::Origin& origin,
    int process_id,
    int frame_id) {
  auto origin_it = origins_.find(origin);
  if (origin_it == origins_.end())
    return;

  OriginState& origin_state = origin_it->second;
  for (auto& grant : origin_state.read_grants)
    grant.second->SetStatus(PermissionStatus::ASK);
  for (auto& grant : origin_state.write_grants)
    grant.second->SetStatus(PermissionStatus::ASK);
  ScheduleUsageIconUpdate();
}

bool OriginScopedNativeFileSystemPermissionContext::OriginHasReadAccess(
    const url::Origin& origin) {
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return false;
  if (it->second.read_grants.empty())
    return false;
  for (const auto& grant : it->second.read_grants) {
    if (grant.second->GetStatus() == PermissionStatus::GRANTED)
      return true;
  }
  return false;
}

bool OriginScopedNativeFileSystemPermissionContext::OriginHasWriteAccess(
    const url::Origin& origin) {
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return false;
  if (it->second.write_grants.empty())
    return false;
  for (const auto& grant : it->second.write_grants) {
    if (grant.second->GetStatus() == PermissionStatus::GRANTED)
      return true;
  }
  return false;
}

void OriginScopedNativeFileSystemPermissionContext::PermissionGrantDestroyed(
    PermissionGrantImpl* grant) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(grant->origin());
  if (it == origins_.end())
    return;

  auto& grants = grant->type() == GrantType::kRead ? it->second.read_grants
                                                   : it->second.write_grants;
  size_t result = grants.erase(grant->path());
  DCHECK_EQ(result, 1u);

  ScheduleUsageIconUpdate();
}

void OriginScopedNativeFileSystemPermissionContext::ScheduleUsageIconUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (usage_icon_update_scheduled_)
    return;
  usage_icon_update_scheduled_ = true;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &OriginScopedNativeFileSystemPermissionContext::DoUsageIconUpdate,
          weak_factory_.GetWeakPtr()));
}

void OriginScopedNativeFileSystemPermissionContext::DoUsageIconUpdate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  usage_icon_update_scheduled_ = false;
#if !defined(OS_ANDROID)
  for (Browser* browser : *BrowserList::GetInstance()) {
    if (browser->profile() != profile())
      continue;
    browser->window()->UpdatePageActionIcon(
        PageActionIconType::kNativeFileSystemAccess);
  }
#endif
}

base::WeakPtr<ChromeNativeFileSystemPermissionContext>
OriginScopedNativeFileSystemPermissionContext::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
