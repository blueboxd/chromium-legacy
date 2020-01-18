// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/download_manager_delegate_impl.h"

#include "base/files/file_util.h"
#include "base/task/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/download_manager.h"
#include "net/base/filename_util.h"
#include "weblayer/browser/browser_context_impl.h"
#include "weblayer/browser/download_impl.h"
#include "weblayer/browser/download_manager_delegate_impl.h"
#include "weblayer/browser/profile_impl.h"
#include "weblayer/browser/tab_impl.h"
#include "weblayer/public/download_delegate.h"

namespace weblayer {

namespace {

void GenerateFilename(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& suggested_filename,
    const std::string& mime_type,
    const base::FilePath& suggested_directory,
    base::OnceCallback<void(const base::FilePath&)> callback) {
  base::FilePath generated_name =
      net::GenerateFileName(url, content_disposition, std::string(),
                            suggested_filename, mime_type, "download");

  if (!base::PathExists(suggested_directory))
    base::CreateDirectory(suggested_directory);

  base::FilePath suggested_path(suggested_directory.Append(generated_name));
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(std::move(callback), suggested_path));
}

}  // namespace

DownloadManagerDelegateImpl::DownloadManagerDelegateImpl(
    content::DownloadManager* download_manager)
    : download_manager_(download_manager) {
  download_manager_->AddObserver(this);
}

DownloadManagerDelegateImpl::~DownloadManagerDelegateImpl() {
  download_manager_->RemoveObserver(this);
}

bool DownloadManagerDelegateImpl::DetermineDownloadTarget(
    download::DownloadItem* item,
    content::DownloadTargetCallback* callback) {
  if (!item->GetForcedFilePath().empty()) {
    std::move(*callback).Run(
        item->GetForcedFilePath(),
        download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
        download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
        download::DownloadItem::MixedContentStatus::UNKNOWN,
        item->GetForcedFilePath(), download::DOWNLOAD_INTERRUPT_REASON_NONE);
    return true;
  }

  auto filename_determined_callback = base::BindOnce(
      &DownloadManagerDelegateImpl::OnDownloadPathGenerated,
      weak_ptr_factory_.GetWeakPtr(), item->GetId(), std::move(*callback));

  auto* browser_context = content::DownloadItemUtils::GetBrowserContext(item);
  base::FilePath default_download_path;
  GetSaveDir(browser_context, nullptr, &default_download_path);

  base::PostTask(FROM_HERE,
                 {base::ThreadPool(), base::MayBlock(),
                  base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN,
                  base::TaskPriority::USER_VISIBLE},
                 base::BindOnce(GenerateFilename, item->GetURL(),
                                item->GetContentDisposition(),
                                item->GetSuggestedFilename(),
                                item->GetMimeType(), default_download_path,
                                std::move(filename_determined_callback)));
  return true;
}

bool DownloadManagerDelegateImpl::InterceptDownloadIfApplicable(
    const GURL& url,
    const std::string& user_agent,
    const std::string& content_disposition,
    const std::string& mime_type,
    const std::string& request_origin,
    int64_t content_length,
    bool is_transient,
    content::WebContents* web_contents) {
  // If there's no DownloadDelegate, the download is simply dropped.
  auto* delegate = GetDelegate(web_contents);
  if (!delegate)
    return true;

  return delegate->InterceptDownload(url, user_agent, content_disposition,
                                     mime_type, content_length);
}

void DownloadManagerDelegateImpl::GetSaveDir(
    content::BrowserContext* browser_context,
    base::FilePath* website_save_dir,
    base::FilePath* download_save_dir) {
  auto* browser_context_impl =
      static_cast<BrowserContextImpl*>(browser_context);
  auto* profile = browser_context_impl->profile_impl();
  if (!profile->download_directory().empty())
    *download_save_dir = profile->download_directory();
}

void DownloadManagerDelegateImpl::CheckDownloadAllowed(
    const content::WebContents::Getter& web_contents_getter,
    const GURL& url,
    const std::string& request_method,
    base::Optional<url::Origin> request_initiator,
    bool from_download_cross_origin_redirect,
    content::CheckDownloadAllowedCallback check_download_allowed_cb) {
  // If there's no DownloadDelegate, the download is simply dropped.
  auto* delegate = GetDelegate(web_contents_getter.Run());
  if (!delegate) {
    std::move(check_download_allowed_cb).Run(false);
    return;
  }

  delegate->AllowDownload(url, request_method, request_initiator,
                          std::move(check_download_allowed_cb));
}

void DownloadManagerDelegateImpl::OnDownloadCreated(
    content::DownloadManager* manager,
    download::DownloadItem* item) {
  item->AddObserver(this);
  // Create a DownloadImpl which will be owned by |item|.
  DownloadImpl::Create(item);

  auto* delegate = GetDelegate(item);
  if (delegate)
    delegate->DownloadStarted(DownloadImpl::Get(item));
}

void DownloadManagerDelegateImpl::OnDownloadDropped(
    content::DownloadManager* manager) {
  if (download_dropped_callback_)
    download_dropped_callback_.Run();
}

void DownloadManagerDelegateImpl::OnDownloadUpdated(
    download::DownloadItem* item) {
  auto* delegate = GetDelegate(item);
  if (item->GetState() == download::DownloadItem::COMPLETE ||
      item->GetState() == download::DownloadItem::CANCELLED ||
      item->GetState() == download::DownloadItem::INTERRUPTED) {
    // Stop observing now to ensure we only send one complete/fail notification.
    item->RemoveObserver(this);

    if (item->GetState() == download::DownloadItem::COMPLETE)
      delegate->DownloadCompleted(DownloadImpl::Get(item));
    else
      delegate->DownloadFailed(DownloadImpl::Get(item));

    // Needs to happen asynchronously to avoid nested observer calls.
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&DownloadManagerDelegateImpl::RemoveItem,
                       weak_ptr_factory_.GetWeakPtr(), item->GetGuid()));
    return;
  }

  if (delegate)
    delegate->DownloadProgressChanged(DownloadImpl::Get(item));
}

void DownloadManagerDelegateImpl::OnDownloadPathGenerated(
    uint32_t download_id,
    content::DownloadTargetCallback callback,
    const base::FilePath& suggested_path) {
  std::move(callback).Run(
      suggested_path, download::DownloadItem::TARGET_DISPOSITION_OVERWRITE,
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
      download::DownloadItem::MixedContentStatus::UNKNOWN,
      suggested_path.AddExtension(FILE_PATH_LITERAL(".crdownload")),
      download::DOWNLOAD_INTERRUPT_REASON_NONE);
}

void DownloadManagerDelegateImpl::RemoveItem(const std::string& guid) {
  auto* item = download_manager_->GetDownloadByGuid(guid);
  if (item)
    item->Remove();
}

DownloadDelegate* DownloadManagerDelegateImpl::GetDelegate(
    content::WebContents* web_contents) {
  if (!web_contents)
    return nullptr;

  auto* tab = TabImpl::FromWebContents(web_contents);
  if (!tab)
    return nullptr;

  return tab->download_delegate();
}

DownloadDelegate* DownloadManagerDelegateImpl::GetDelegate(
    download::DownloadItem* item) {
  auto* web_contents = content::DownloadItemUtils::GetWebContents(item);
  return GetDelegate(web_contents);
}

}  // namespace weblayer
