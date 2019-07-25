// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_resource_getter_impl.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/url_constants.h"
#include "media/base/android/media_url_interceptor.h"
#include "net/base/auth.h"
#include "net/http/http_auth.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

// Returns the cookie manager for the |browser_context| at the client end of the
// mojo pipe. This will be restricted to the origin of |url|, and will apply
// policies from user and ContentBrowserClient to cookie operations.
network::mojom::RestrictedCookieManagerPtr GetRestrictedCookieManagerForContext(
    BrowserContext* browser_context,
    const GURL& url,
    int render_process_id,
    int render_frame_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  url::Origin origin = url::Origin::Create(url);
  StoragePartition* storage_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);

  network::mojom::RestrictedCookieManagerPtr pipe;
  network::mojom::RestrictedCookieManagerRequest request =
      mojo::MakeRequest(&pipe);
  storage_partition->CreateRestrictedCookieManager(
      network::mojom::RestrictedCookieManagerRole::NETWORK, origin,
      /* is_service_worker = */ false, render_process_id, render_frame_id,
      std::move(request));
  return pipe;
}

void ReturnResultOnUIThread(
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& result) {
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

void ReturnResultOnUIThreadAndClosePipe(
    network::mojom::RestrictedCookieManagerPtr pipe,
    base::OnceCallback<void(const std::string&)> callback,
    const std::string& result) {
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

void OnSyncGetPlatformPathDone(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    media::MediaResourceGetter::GetPlatformPathCB callback,
    const base::FilePath& platform_path) {
  DCHECK(file_system_context->default_file_task_runner()
             ->RunsTasksInCurrentSequence());

  base::FilePath data_storage_path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &data_storage_path);
  if (data_storage_path.IsParent(platform_path))
    ReturnResultOnUIThread(std::move(callback), platform_path.value());
  else
    ReturnResultOnUIThread(std::move(callback), std::string());
}

void RequestPlatformPathFromFileSystemURL(
    const GURL& url,
    int render_process_id,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    media::MediaResourceGetter::GetPlatformPathCB callback) {
  DCHECK(file_system_context->default_file_task_runner()
             ->RunsTasksInCurrentSequence());
  SyncGetPlatformPath(file_system_context.get(), render_process_id, url,
                      base::BindOnce(&OnSyncGetPlatformPathDone,
                                     file_system_context, std::move(callback)));
}

}  // namespace

MediaResourceGetterImpl::MediaResourceGetterImpl(
    BrowserContext* browser_context,
    storage::FileSystemContext* file_system_context,
    int render_process_id,
    int render_frame_id)
    : browser_context_(browser_context),
      file_system_context_(file_system_context),
      render_process_id_(render_process_id),
      render_frame_id_(render_frame_id),
      weak_factory_(this) {
}

MediaResourceGetterImpl::~MediaResourceGetterImpl() {}

void MediaResourceGetterImpl::GetAuthCredentials(
    const GURL& url,
    GetAuthCredentialsCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Non-standard URLs, such as data, will not be found in HTTP auth cache
  // anyway, because they have no valid origin, so don't waste the time.
  if (!url.IsStandard()) {
    GetAuthCredentialsCallback(std::move(callback), base::nullopt);
    return;
  }

  BrowserContext::GetDefaultStoragePartition(browser_context_)
      ->GetNetworkContext()
      ->LookupBasicAuthCredentials(
          url,
          base::BindOnce(&MediaResourceGetterImpl::GetAuthCredentialsCallback,
                         weak_factory_.GetWeakPtr(), std::move(callback)));
}

void MediaResourceGetterImpl::GetCookies(const GURL& url,
                                         const GURL& site_for_cookies,
                                         GetCookieCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, url)) {
    // Running the callback asynchronously on the caller thread to avoid
    // reentrancy issues.
    ReturnResultOnUIThread(std::move(callback), std::string());
    return;
  }

  network::mojom::RestrictedCookieManagerPtr cookie_manager =
      GetRestrictedCookieManagerForContext(
          browser_context_, url, render_process_id_, render_frame_id_);
  network::mojom::RestrictedCookieManager* cookie_manager_ptr =
      cookie_manager.get();
  cookie_manager_ptr->GetCookiesString(
      url, site_for_cookies,
      base::BindOnce(&ReturnResultOnUIThreadAndClosePipe,
                     std::move(cookie_manager), std::move(callback)));
}

void MediaResourceGetterImpl::GetAuthCredentialsCallback(
    GetAuthCredentialsCB callback,
    const base::Optional<net::AuthCredentials>& credentials) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (credentials)
    std::move(callback).Run(credentials->username(), credentials->password());
  else
    std::move(callback).Run(base::string16(), base::string16());
}

void MediaResourceGetterImpl::GetPlatformPathFromURL(
    const GURL& url,
    GetPlatformPathCB callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(url.SchemeIsFileSystem());

  GetPlatformPathCB cb =
      base::BindOnce(&MediaResourceGetterImpl::GetPlatformPathCallback,
                     weak_factory_.GetWeakPtr(), std::move(callback));

  scoped_refptr<storage::FileSystemContext> context(file_system_context_);
  context->default_file_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&RequestPlatformPathFromFileSystemURL, url,
                                render_process_id_, context, std::move(cb)));
}

void MediaResourceGetterImpl::GetPlatformPathCallback(
    GetPlatformPathCB callback,
    const std::string& platform_path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(callback).Run(platform_path);
}

}  // namespace content
