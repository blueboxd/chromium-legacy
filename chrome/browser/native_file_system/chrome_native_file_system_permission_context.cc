// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/native_file_system/chrome_native_file_system_permission_context.h"

#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
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
#include "components/safe_browsing/buildflags.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"

namespace {

void ShowDirectoryAccessConfirmationPromptOnUIThread(
    int process_id,
    int frame_id,
    const url::Origin& origin,
    const base::FilePath& path,
    base::OnceCallback<void(permissions::PermissionAction result)> callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(process_id, frame_id);
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(rfh);

  if (!web_contents) {
    // Requested from a worker, or a no longer existing tab.
    std::move(callback).Run(permissions::PermissionAction::DISMISSED);
    return;
  }

  // Drop fullscreen mode so that the user sees the URL bar.
  base::ScopedClosureRunner fullscreen_block =
      web_contents->ForSecurityDropFullscreen();

  ShowNativeFileSystemDirectoryAccessConfirmationDialog(
      origin, path, std::move(callback), web_contents,
      std::move(fullscreen_block));
}

void ShowNativeFileSystemRestrictedDirectoryDialogOnUIThread(
    int process_id,
    int frame_id,
    const url::Origin& origin,
    const base::FilePath& path,
    bool is_directory,
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
      origin, path, is_directory, std::move(callback), web_contents);
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
  // parents are blocked. If a blocked path is a descendent of another blocked
  // path, then it may override the child-blocking policy of its ancestor. For
  // example, if /home blocks all children, but /home/downloads does not, then
  // /home/downloads/file.ext should *not* be blocked.
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

  base::FilePath nearest_ancestor;
  bool nearest_ancestor_blocks_all_children = false;
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

    if (blocked_path.IsParent(check_path) &&
        (nearest_ancestor.empty() || nearest_ancestor.IsParent(blocked_path))) {
      nearest_ancestor = blocked_path;
      nearest_ancestor_blocks_all_children = block.block_all_children;
    }
  }

  return !nearest_ancestor.empty() && nearest_ancestor_blocks_all_children;
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

void DoSafeBrowsingCheckOnUIThread(
    int process_id,
    int frame_id,
    std::unique_ptr<content::NativeFileSystemWriteItem> item,
    safe_browsing::CheckDownloadCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Download Protection Service is not supported on Android.
#if BUILDFLAG(FULL_SAFE_BROWSING)
  safe_browsing::SafeBrowsingService* sb_service =
      g_browser_process->safe_browsing_service();
  if (!sb_service || !sb_service->download_protection_service() ||
      !sb_service->download_protection_service()->enabled()) {
    std::move(callback).Run(safe_browsing::DownloadCheckResult::UNKNOWN);
    return;
  }

  if (!item->browser_context) {
    content::RenderProcessHost* rph =
        content::RenderProcessHost::FromID(process_id);
    if (!rph) {
      std::move(callback).Run(safe_browsing::DownloadCheckResult::UNKNOWN);
      return;
    }
    item->browser_context = rph->GetBrowserContext();
  }

  if (!item->web_contents) {
    content::RenderFrameHost* rfh =
        content::RenderFrameHost::FromID(process_id, frame_id);
    if (rfh)
      item->web_contents = content::WebContents::FromRenderFrameHost(rfh);
  }

  sb_service->download_protection_service()->CheckNativeFileSystemWrite(
      std::move(item), std::move(callback));
#endif
}

ChromeNativeFileSystemPermissionContext::AfterWriteCheckResult
InterpretSafeBrowsingResult(safe_browsing::DownloadCheckResult result) {
  using Result = safe_browsing::DownloadCheckResult;
  switch (result) {
    // Only allow downloads that are marked as SAFE or UNKNOWN by SafeBrowsing.
    // All other types are going to be blocked. UNKNOWN could be the result of a
    // failed safe browsing ping.
    case Result::UNKNOWN:
    case Result::SAFE:
    case Result::WHITELISTED_BY_POLICY:
      return ChromeNativeFileSystemPermissionContext::AfterWriteCheckResult::
          kAllow;

    case Result::DANGEROUS:
    case Result::UNCOMMON:
    case Result::DANGEROUS_HOST:
    case Result::POTENTIALLY_UNWANTED:
    case Result::BLOCKED_PASSWORD_PROTECTED:
    case Result::BLOCKED_TOO_LARGE:
    case Result::BLOCKED_UNSUPPORTED_FILE_TYPE:
      return ChromeNativeFileSystemPermissionContext::AfterWriteCheckResult::
          kBlock;

    // This shouldn't be returned for Native File System write checks.
    case Result::ASYNC_SCANNING:
    case Result::SENSITIVE_CONTENT_WARNING:
    case Result::SENSITIVE_CONTENT_BLOCK:
    case Result::DEEP_SCANNED_SAFE:
    case Result::PROMPT_FOR_SCANNING:
      NOTREACHED();
      return ChromeNativeFileSystemPermissionContext::AfterWriteCheckResult::
          kAllow;
  }
  NOTREACHED();
  return ChromeNativeFileSystemPermissionContext::AfterWriteCheckResult::kBlock;
}

}  // namespace

ChromeNativeFileSystemPermissionContext::Grants::Grants() = default;
ChromeNativeFileSystemPermissionContext::Grants::~Grants() = default;
ChromeNativeFileSystemPermissionContext::Grants::Grants(Grants&&) = default;
ChromeNativeFileSystemPermissionContext::Grants&
ChromeNativeFileSystemPermissionContext::Grants::operator=(Grants&&) = default;

ChromeNativeFileSystemPermissionContext::
    ChromeNativeFileSystemPermissionContext(content::BrowserContext* context) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  auto* profile = Profile::FromBrowserContext(context);
  content_settings_ = base::WrapRefCounted(
      HostContentSettingsMapFactory::GetForProfile(profile));
}

ChromeNativeFileSystemPermissionContext::
    ~ChromeNativeFileSystemPermissionContext() = default;

ContentSetting
ChromeNativeFileSystemPermissionContext::GetWriteGuardContentSetting(
    const url::Origin& origin) {
  return content_settings()->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::NATIVE_FILE_SYSTEM_WRITE_GUARD,
      /*provider_id=*/std::string());
}

ContentSetting
ChromeNativeFileSystemPermissionContext::GetReadGuardContentSetting(
    const url::Origin& origin) {
  return content_settings()->GetContentSetting(
      origin.GetURL(), origin.GetURL(),
      ContentSettingsType::NATIVE_FILE_SYSTEM_READ_GUARD,
      /*provider_id=*/std::string());
}

bool ChromeNativeFileSystemPermissionContext::CanObtainWritePermission(
    const url::Origin& origin) {
  return GetWriteGuardContentSetting(origin) == CONTENT_SETTING_ASK ||
         GetWriteGuardContentSetting(origin) == CONTENT_SETTING_ALLOW;
}

void ChromeNativeFileSystemPermissionContext::ConfirmDirectoryReadAccess(
    const url::Origin& origin,
    const base::FilePath& path,
    int process_id,
    int frame_id,
    base::OnceCallback<void(PermissionStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &ShowDirectoryAccessConfirmationPromptOnUIThread, process_id,
          frame_id, origin, path,
          base::BindOnce(
              [](scoped_refptr<base::TaskRunner> task_runner,
                 base::OnceCallback<void(PermissionStatus result)> callback,
                 permissions::PermissionAction result) {
                task_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        std::move(callback),
                        result == permissions::PermissionAction::GRANTED
                            ? PermissionStatus::GRANTED
                            : PermissionStatus::DENIED));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void ChromeNativeFileSystemPermissionContext::ConfirmSensitiveDirectoryAccess(
    const url::Origin& origin,
    const std::vector<base::FilePath>& paths,
    bool is_directory,
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
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ShouldBlockAccessToPath, paths[0]),
      base::BindOnce(&ChromeNativeFileSystemPermissionContext::
                         DidConfirmSensitiveDirectoryAccess,
                     GetWeakPtr(), origin, paths, is_directory, process_id,
                     frame_id, std::move(callback)));
}

void ChromeNativeFileSystemPermissionContext::PerformAfterWriteChecks(
    std::unique_ptr<content::NativeFileSystemWriteItem> item,
    int process_id,
    int frame_id,
    base::OnceCallback<void(AfterWriteCheckResult)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(
          &DoSafeBrowsingCheckOnUIThread, process_id, frame_id, std::move(item),
          base::BindOnce(
              [](scoped_refptr<base::TaskRunner> task_runner,
                 base::OnceCallback<void(AfterWriteCheckResult result)>
                     callback,
                 safe_browsing::DownloadCheckResult result) {
                task_runner->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback),
                                   InterpretSafeBrowsingResult(result)));
              },
              base::SequencedTaskRunnerHandle::Get(), std::move(callback))));
}

void ChromeNativeFileSystemPermissionContext::
    DidConfirmSensitiveDirectoryAccess(
        const url::Origin& origin,
        const std::vector<base::FilePath>& paths,
        bool is_directory,
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

  base::PostTask(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&ShowNativeFileSystemRestrictedDirectoryDialogOnUIThread,
                     process_id, frame_id, origin, paths[0], is_directory,
                     std::move(result_callback)));
}

bool ChromeNativeFileSystemPermissionContext::OriginHasReadAccess(
    const url::Origin& origin) {
  NOTREACHED();
  return false;
}

bool ChromeNativeFileSystemPermissionContext::OriginHasWriteAccess(
    const url::Origin& origin) {
  NOTREACHED();
  return false;
}
