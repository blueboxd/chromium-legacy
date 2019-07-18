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
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(process_id, frame_id);
  if (!rfh || !rfh->IsCurrent()) {
    // Requested from a no longer valid render frame host.
    std::move(callback).Run();
    return;
  }

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);
  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run();
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
base::OnceCallback<void(PermissionAction result)>
BindPermissionActionCallbackToCurrentSequence(
    base::OnceCallback<void(PermissionAction result)> callback) {
  return base::BindOnce(
      [](scoped_refptr<base::TaskRunner> task_runner,
         base::OnceCallback<void(PermissionAction result)> callback,
         PermissionAction result) {
        task_runner->PostTask(FROM_HERE,
                              base::BindOnce(std::move(callback), result));
      },
      base::SequencedTaskRunnerHandle::Get(), std::move(callback));
}

}  // namespace

ChromeNativeFileSystemPermissionContext::Grants::Grants() = default;
ChromeNativeFileSystemPermissionContext::Grants::~Grants() = default;
ChromeNativeFileSystemPermissionContext::Grants::Grants(Grants&&) = default;
ChromeNativeFileSystemPermissionContext::Grants&
ChromeNativeFileSystemPermissionContext::Grants::operator=(Grants&&) = default;

class ChromeNativeFileSystemPermissionContext::PermissionGrantImpl
    : public content::NativeFileSystemPermissionGrant {
 public:
  PermissionGrantImpl(
      scoped_refptr<ChromeNativeFileSystemPermissionContext> context,
      const url::Origin& origin,
      const base::FilePath& path,
      bool is_directory)
      : context_(std::move(context)),
        origin_(origin),
        path_(path),
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
    return path_;
  }

  PermissionStatus GetStatus() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return status_;
  }

  void SetStatus(PermissionStatus status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    status_ = status;
  }

  void RequestPermission(int process_id,
                         int frame_id,
                         base::OnceClosure callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (GetStatus() != PermissionStatus::ASK) {
      std::move(callback).Run();
      return;
    }

    auto result_callback = BindPermissionActionCallbackToCurrentSequence(
        base::BindOnce(&PermissionGrantImpl::OnPermissionRequestComplete, this,
                       std::move(callback)));

    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&ShowWritePermissionPromptOnUIThread, process_id,
                       frame_id, origin_, path_, is_directory_,
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
        status_ = PermissionStatus::GRANTED;
        break;
      case PermissionAction::DENIED:
        status_ = PermissionStatus::DENIED;
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
  const base::FilePath path_;
  const bool is_directory_;
  PermissionStatus status_ = PermissionStatus::ASK;

  DISALLOW_COPY_AND_ASSIGN(PermissionGrantImpl);
};

struct ChromeNativeFileSystemPermissionContext::OriginState {
  // Raw pointers, owned collectively by all the handles that reference this
  // grant. When last reference goes away this state is cleared as well by
  // PermissionGrantDestroyed().
  // TODO(mek): Lifetime of grants might change depending on the outcome of
  // the discussions surrounding lifetime of non-persistent permissions.
  std::map<base::FilePath, PermissionGrantImpl*> grants;
};

ChromeNativeFileSystemPermissionContext::
    ChromeNativeFileSystemPermissionContext(content::BrowserContext*) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

scoped_refptr<content::NativeFileSystemPermissionGrant>
ChromeNativeFileSystemPermissionContext::GetWritePermissionGrant(
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
    UserAction user_action) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // operator[] might insert a new OriginState in |origins_|, but that is
  // exactly what we want.
  auto& origin_state = origins_[origin];
  // TODO(mek): Do some kind of clever merging of grants, for example
  // returning a parent directory's grant if that one already has write
  // access.
  auto*& existing_grant = origin_state.grants[path];
  if (existing_grant) {
    if (user_action == UserAction::kSave)
      existing_grant->SetStatus(PermissionGrantImpl::PermissionStatus::GRANTED);
    return existing_grant;
  }
  auto result = base::MakeRefCounted<PermissionGrantImpl>(this, origin, path,
                                                          is_directory);
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
    const url::Origin& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Grants grants;
  auto it = origins_.find(origin);
  if (it == origins_.end())
    return grants;
  for (const auto& entry : it->second.grants) {
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
          permission_context, origin),
      std::move(callback));
}

ChromeNativeFileSystemPermissionContext::
    ~ChromeNativeFileSystemPermissionContext() = default;

void ChromeNativeFileSystemPermissionContext::ShutdownOnUIThread() {}

void ChromeNativeFileSystemPermissionContext::PermissionGrantDestroyed(
    PermissionGrantImpl* grant) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto it = origins_.find(grant->origin());
  DCHECK(it != origins_.end());
  it->second.grants.erase(grant->path());
  if (it->second.grants.empty()) {
    // No more grants for the origin, so remove the state.
    origins_.erase(it);
  }
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

  auto result_callback = base::BindOnce(
      [](scoped_refptr<base::TaskRunner> task_runner,
         base::OnceClosure callback) {
        task_runner->PostTask(FROM_HERE, std::move(callback));
      },
      base::SequencedTaskRunnerHandle::Get(),
      base::BindOnce(std::move(callback), SensitiveDirectoryResult::kTryAgain));

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&ShowNativeFileSystemRestrictedDirectoryDialogOnUIThread,
                     process_id, frame_id, origin, paths[0],
                     std::move(result_callback)));
}
