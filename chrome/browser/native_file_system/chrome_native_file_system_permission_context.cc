// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"

#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "chrome/browser/native_file_system/native_file_system_permission_context_factory.h"
#include "chrome/browser/native_file_system/native_file_system_permission_request_manager.h"
#include "chrome/browser/permissions/permission_util.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace {

void ShowWritePermissionPromptOnUIThread(
    int process_id,
    int frame_id,
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    base::OnceCallback<void(PermissionAction result)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(process_id, frame_id);
  if (!rfh || !rfh->IsCurrent()) {
    // Requested from a no longer valid render frame host.
    std::move(callback).Run(PermissionAction::DISMISSED);
    return;
  }

  if (!rfh->HasTransientUserActivation()) {
    // No permission prompts without user activation.
    std::move(callback).Run(PermissionAction::DISMISSED);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run(PermissionAction::DISMISSED);
    return;
  }

  url::Origin embedding_origin =
      url::Origin::Create(web_contents->GetLastCommittedURL());
  if (embedding_origin != origin) {
    // Third party iframes are not allowed to request more permissions.
    std::move(callback).Run(PermissionAction::DISMISSED);
    return;
  }

  auto* request_manager =
      NativeFileSystemPermissionRequestManager::FromWebContents(web_contents);
  if (!request_manager) {
    std::move(callback).Run(PermissionAction::DISMISSED);
    return;
  }

  request_manager->AddRequest({origin, path, is_directory},
                              std::move(callback));
}

void ShowDirectoryAccessConfirmationPromptOnUIThread(
    int process_id,
    int frame_id,
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(PermissionAction result)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(process_id, frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run(PermissionAction::DISMISSED);
  }

  ShowNativeFileSystemDirectoryAccessConfirmationDialog(
      origin, path, std::move(callback), web_contents);
}

void ShowNativeFileSystemRestrictedDirectoryDialogOnUIThread(
    int process_id,
    int frame_id,
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<
        void(ChromeNativeFileSystemPermissionContext::SensitiveDirectoryResult)>
        callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(process_id, frame_id);
  if (!rfh || !rfh->IsCurrent()) {
    // Requested from a no longer valid render frame host.
    std::move(callback).Run(ChromeNativeFileSystemPermissionContext::
                                SensitiveDirectoryResult::kAbort);
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run(ChromeNativeFileSystemPermissionContext::
                                SensitiveDirectoryResult::kAbort);
    return;
  }

  ShowNativeFileSystemRestrictedDirectoryDialog(
      origin, path, std::move(callback), web_contents);
}

// Sentinel used to indicate that no PathService key is specified for a path in
// the struct below.
constexpr const int kNoBasePathKey = -1;

const struct {
  // base::BasePathKey value (or one of the platform specific extensions to it)
  // for a path that should be blocked. Specify kNoBasePathKey if |path| should
  // be used instead.
  int base_path_key;

  // Explicit path to block instead of using |base_path_key|. Set to nullptr to
  // use |base_path_key| on its own. If both |base_path_key| and |path| are set,
  // |path| is treated relative to the path |base_path_key| resolves to.
  const base::FilePath::CharType* path;

  // If this is set to true not only is the given path blocked, all the children
  // are blocked as well. When this is set to false only the path and its
  // parents are blocked.
  bool block_all_children;
} kBlockedPaths[] = {
    // Don't allow users to share their entire home directory, entire desktop or
    // entire documents folder, but do allow sharing anything inside those
    // directories not otherwise blocked.
    {base::DIR_HOME, nullptr, false},
    {base::DIR_USER_DESKTOP, nullptr, false},
    {chrome::DIR_USER_DOCUMENTS, nullptr, false},
    // Similar restrictions for the downloads directory.
    {chrome::DIR_DEFAULT_DOWNLOADS, nullptr, false},
    {chrome::DIR_DEFAULT_DOWNLOADS_SAFE, nullptr, false},
    // The Chrome installation itself should not be modified by the web.
    {chrome::DIR_APP, nullptr, true},
    // And neither should the configuration of at least the currently running
    // Chrome instance (note that this does not take --user-data-dir command
    // line overrides into account).
    {chrome::DIR_USER_DATA, nullptr, true},
    // ~/.ssh is pretty sensitive on all platforms, so block access to that.
    {base::DIR_HOME, FILE_PATH_LITERAL(".ssh"), true},
    // And limit access to ~/.gnupg as well.
    {base::DIR_HOME, FILE_PATH_LITERAL(".gnupg"), true},
#if defined(OS_WIN)
    // Some Windows specific directories to block, basically all apps, the
    // operating system itself, as well as configuration data for apps.
    {base::DIR_PROGRAM_FILES, nullptr, true},
    {base::DIR_PROGRAM_FILESX86, nullptr, true},
    {base::DIR_PROGRAM_FILES6432, nullptr, true},
    {base::DIR_WINDOWS, nullptr, true},
    {base::DIR_APP_DATA, nullptr, true},
    {base::DIR_LOCAL_APP_DATA, nullptr, true},
    {base::DIR_COMMON_APP_DATA, nullptr, true},
#endif
#if defined(OS_MACOSX)
    // Similar Mac specific blocks.
    {base::DIR_APP_DATA, nullptr, true},
    {base::DIR_HOME, FILE_PATH_LITERAL("Library"), true},
#endif
#if defined(OS_LINUX)
    // On Linux also block access to devices via /dev, as well as security
    // sensitive data in /sys and /proc.
    {kNoBasePathKey, FILE_PATH_LITERAL("/dev"), true},
    {kNoBasePathKey, FILE_PATH_LITERAL("/sys"), true},
    {kNoBasePathKey, FILE_PATH_LITERAL("/proc"), true},
    // And block all of ~/.config, matching the similar restrictions on mac
    // and windows.
    {base::DIR_HOME, FILE_PATH_LITERAL(".config"), true},
    // Block ~/.dbus as well, just in case, although there probably isn't much a
    // website can do with access to that directory and its contents.
    {base::DIR_HOME, FILE_PATH_LITERAL(".dbus"), true},
#endif
    // TODO(https://crbug.com/984641): Refine this list, for example add
    // XDG_CONFIG_HOME when it is not set ~/.config?
};

bool ShouldBlockAccessToPath(const base::FilePath& check_path) {
  DCHECK(!check_path.empty());
  DCHECK(check_path.IsAbsolute());
  for (const auto& block : kBlockedPaths) {
    base::FilePath blocked_path;
    if (block.base_path_key != kNoBasePathKey) {
      if (!base::PathService::Get(block.base_path_key, &blocked_path))
        continue;
      if (block.path)
        blocked_path = blocked_path.Append(block.path);
    } else {
      DCHECK(block.path);
      blocked_path = base::FilePath(block.path);
    }

    if (check_path == blocked_path || check_path.IsParent(blocked_path))
      return true;

    if (block.block_all_children && blocked_path.IsParent(check_path))
      return true;
  }
  return false;
}

// Returns a callback that calls the passed in |callback| by posting a task to
// the current sequenced task runner.
template <typename ResultType>
base::OnceCallback<void(ResultType result)> BindResultCallbackToCurrentSequence(
    base::OnceCallback<void(ResultType result)> callback) {
  return base::BindOnce(
      [](scoped_refptr<base::TaskRunner> task_runner,
         base::OnceCallback<void(ResultType result)> callback,
         ResultType result) {
        task_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), result));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));
}

// Read permissions start out as granted, and can change to denied if the user
// revokes them for a tab. Re-requesting read permission is not currently
// supported.
class ReadPermissionGrantImpl
    : public content::NativeFileSystemPermissionGrant {
 public:
  ReadPermissionGrantImpl() {}

  // NativeFileSystemPermissionGrant:
  PermissionStatus GetStatus() override { return status_; }
  void RequestPermission(int process_id,
                         int frame_id,
                         base::OnceClosure callback) override {
    // Requesting read permission is not currently supported, so just call the
    // callback right away.
    std::move(callback).Run();
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

ChromeNativeFileSystemPermissionContext::Grants::Grants() = default;
ChromeNativeFileSystemPermissionContext::Grants::~Grants() = default;
ChromeNativeFileSystemPermissionContext::Grants::Grants(Grants&&) = default;
ChromeNativeFileSystemPermissionContext::Grants&
ChromeNativeFileSystemPermissionContext::Grants::operator=(Grants&&) = default;

class ChromeNativeFileSystemPermissionContext::PermissionGrantImpl
    : public content::NativeFileSystemPermissionGrant {
 public:
  // In the current implementation permission grants are scoped to the frame
  // they are requested in. Within a frame we only want to have one grant pe
  // path. The Key struct contains these fields. Keys are comparable so they can
  // be used with sorted containers like std::map and std::set.
  // TODO(https://crbug.com/984769): Eliminate process_id and frame_id and
  // replace usage of this struct with just a file path when grants stop being
  // scoped to a frame.
  struct Key {
    base::FilePath path;
    int process_id = 0;
    int frame_id = 0;

    bool operator==(const Key& rhs) const {
      return std::tie(process_id, frame_id, path) ==
             std::tie(rhs.process_id, rhs.frame_id, rhs.path);
    }
    bool operator<(const Key& rhs) const {
      return std::tie(process_id, frame_id, path) <
             std::tie(rhs.process_id, rhs.frame_id, rhs.path);
    }
  };

  PermissionGrantImpl(
      scoped_refptr<ChromeNativeFileSystemPermissionContext> context,
      const url::Origin& origin,
      const Key& key,
      bool is_directory)
      : context_(std::move(context)),
        origin_(origin),
        key_(key),
        is_directory_(is_directory) {}

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
    return key_.path;
  }

  const Key& key() const { return key_; }

  PermissionStatus GetStatus() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return status_;
  }

  void SetStatus(PermissionStatus status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (status_ == status)
      return;
    status_ = status;
    NotifyPermissionStatusChanged();
  }

  void RequestPermission(int process_id,
                         int frame_id,
                         base::OnceClosure callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (GetStatus() != PermissionStatus::ASK) {
      std::move(callback).Run();
      return;
    }

    auto result_callback = BindResultCallbackToCurrentSequence(
        base::BindOnce(&PermissionGrantImpl::OnPermissionRequestComplete, this,
                       std::move(callback)));

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ShowWritePermissionPromptOnUIThread, process_id,
                       frame_id, origin_, path(), is_directory_,
                       std::move(result_callback)));
  }

 protected:
  ~PermissionGrantImpl() override { context_->PermissionGrantDestroyed(this); }

 private:
  void OnPermissionRequestComplete(base::OnceClosure callback,
                                   PermissionAction result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    switch (result) {
      case PermissionAction::GRANTED:
        SetStatus(PermissionStatus::GRANTED);
        break;
      case PermissionAction::DENIED:
        SetStatus(PermissionStatus::DENIED);
        break;
      case PermissionAction::DISMISSED:
      case PermissionAction::IGNORED:
        break;
      case PermissionAction::REVOKED:
      case PermissionAction::NUM:
        NOTREACHED();
        break;
    }

    std::move(callback).Run();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  scoped_refptr<ChromeNativeFileSystemPermissionContext> const context_;
  const url::Origin origin_;
  const Key key_;
  const bool is_directory_;

  // This member should only be updated via SetStatus(), to make sure observers
  // are properly notified about any change in status.
  PermissionStatus status_ = PermissionStatus::ASK;

  DISALLOW_COPY_AND_ASSIGN(PermissionGrantImpl);
};

struct ChromeNativeFileSystemPermissionContext::OriginState {
  // Raw pointers, owned collectively by all the handles that reference this
  // grant. When last reference goes away this state is cleared as well by
  // PermissionGrantDestroyed().
  // TODO(mek): Lifetime of grants might change depending on the outcome of
  // the discussions surrounding lifetime of non-persistent permissions.
  std::map<PermissionGrantImpl::Key, PermissionGrantImpl*> grants;

  // Read permissions are keyed off of the tab they are associated with.
  // TODO(https://crbug.com/984769): This will change when permissions are no
  // longer scoped to individual tabs.
  using ReadPermissionKey = std::pair<int, int>;
  std::map<ReadPermissionKey, scoped_refptr<ReadPermissionGrantImpl>>
      directory_read_grants;
};

ChromeNativeFileSystemPermissionContext::
    ChromeNativeFileSystemPermissionContext(content::BrowserContext*) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

scoped_refptr<content::NativeFileSystemPermissionGrant>
ChromeNativeFileSystemPermissionContext::GetReadPermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    int process_id,
    int frame_id) {
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
ChromeNativeFileSystemPermissionContext::GetWritePermissionGrant(
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
  PermissionGrantImpl::Key grant_key{path, process_id, frame_id};
  auto*& existing_grant = origin_state.grants[grant_key];
  if (existing_grant) {
    if (user_action == UserAction::kSave)
      existing_grant->SetStatus(PermissionGrantImpl::PermissionStatus::GRANTED);
    return existing_grant;
  }
  auto result = base::MakeRefCounted<PermissionGrantImpl>(
      this, origin, grant_key, is_directory);
  if (user_action == UserAction::kSave)
    result->SetStatus(PermissionGrantImpl::PermissionStatus::GRANTED);
  existing_grant = result.get();
  return result;
}

void ChromeNativeFileSystemPermissionContext::ConfirmDirectoryReadAccess(
    const url::Origin& origin,
    const base::FilePath& path,
    int process_id,
    int frame_id,
    base::OnceCallback<void(PermissionStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &ShowDirectoryAccessConfirmationPromptOnUIThread, process_id,
          frame_id, origin, path,
          base::BindOnce(
              [](scoped_refptr<base::TaskRunner> task_runner,
                 base::OnceCallback<void(PermissionStatus result)> callback,
                 PermissionAction result) {
                task_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback),
                                   result == PermissionAction::GRANTED
                                       ? PermissionStatus::GRANTED
                                       : PermissionStatus::DENIED));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void ChromeNativeFileSystemPermissionContext::ConfirmSensitiveDirectoryAccess(
    const url::Origin& origin,
    const std::vector<base::FilePath>& paths,
    int process_id,
    int frame_id,
    base::OnceCallback<void(SensitiveDirectoryResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (paths.empty()) {
    std::move(callback).Run(SensitiveDirectoryResult::kAllowed);
    return;
  }
  // It is enough to only verify access to the first path, as multiple
  // file selection is only supported if all files are in the same
  // directory.
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ShouldBlockAccessToPath, paths[0]),
      base::BindOnce(&ChromeNativeFileSystemPermissionContext::
                         DidConfirmSensitiveDirectoryAccess,
                     this, origin, paths, process_id, frame_id,
                     std::move(callback)));
}

ChromeNativeFileSystemPermissionContext::Grants
ChromeNativeFileSystemPermissionContext::GetPermissionGrants(
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
        PermissionGrantImpl::PermissionStatus::GRANTED) {
      if (entry.second->is_directory()) {
        grants.directory_write_grants.push_back(entry.second->path());
      } else {
        grants.file_write_grants.push_back(entry.second->path());
      }
    }
  }
  return grants;
}

// static
void ChromeNativeFileSystemPermissionContext::GetPermissionGrantsFromUIThread(
    content::BrowserContext* browser_context,
    const url::Origin& origin,
    int process_id,
    int frame_id,
    base::OnceCallback<void(Grants)> callback) {
  auto permission_context =
      NativeFileSystemPermissionContextFactory::GetForProfileIfExists(
          browser_context);
  if (!permission_context) {
    std::move(callback).Run(Grants());
    return;
  }
  base::PostTaskAndReplyWithResult(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          &ChromeNativeFileSystemPermissionContext::GetPermissionGrants,
          permission_context, origin, process_id, frame_id),
      std::move(callback));
}

void ChromeNativeFileSystemPermissionContext::RevokeDirectoryReadGrants(
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

void ChromeNativeFileSystemPermissionContext::RevokeWriteGrants(
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
      PermissionGrantImpl::Key{base::FilePath(), process_id, frame_id});
  while (grant_it != origin_state.grants.end() &&
         grant_it->first.process_id == process_id &&
         grant_it->first.frame_id == frame_id) {
    grant_it->second->SetStatus(PermissionStatus::ASK);
    ++grant_it;
  }
}

// static
void ChromeNativeFileSystemPermissionContext::
    RevokeGrantsForOriginAndTabFromUIThread(
        content::BrowserContext* browser_context,
        const url::Origin& origin,
        int process_id,
        int frame_id) {
  auto permission_context =
      NativeFileSystemPermissionContextFactory::GetForProfileIfExists(
          browser_context);
  if (!permission_context) {
    // With no context there is nothing to revoke.
    return;
  }
  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(
          [](const scoped_refptr<ChromeNativeFileSystemPermissionContext>&
                 context,
             const url::Origin& origin, int process_id, int frame_id) {
            context->RevokeDirectoryReadGrants(origin, process_id, frame_id);
            context->RevokeWriteGrants(origin, process_id, frame_id);
          },
          std::move(permission_context), origin, process_id, frame_id));
}

void ChromeNativeFileSystemPermissionContext::ShutdownOnUIThread() {}

ChromeNativeFileSystemPermissionContext::
    ~ChromeNativeFileSystemPermissionContext() = default;

void ChromeNativeFileSystemPermissionContext::PermissionGrantDestroyed(
    PermissionGrantImpl* grant) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(grant->origin());
  DCHECK(it != origins_.end());
  it->second.grants.erase(grant->key());
}

void ChromeNativeFileSystemPermissionContext::
    DidConfirmSensitiveDirectoryAccess(
        const url::Origin& origin,
        const std::vector<base::FilePath>& paths,
        int process_id,
        int frame_id,
        base::OnceCallback<void(SensitiveDirectoryResult)> callback,
        bool should_block) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!should_block) {
    std::move(callback).Run(SensitiveDirectoryResult::kAllowed);
    return;
  }

  auto result_callback =
      BindResultCallbackToCurrentSequence(std::move(callback));

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&ShowNativeFileSystemRestrictedDirectoryDialogOnUIThread,
                     process_id, frame_id, origin, paths[0],
                     std::move(result_callback)));
}
