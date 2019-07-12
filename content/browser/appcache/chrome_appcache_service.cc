// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/chrome_appcache_service.h"

#include <utility>

#include "base/files/file_path.h"
#include "content/browser/appcache/appcache_storage_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"

namespace content {

ChromeAppCacheService::ChromeAppCacheService(
    storage::QuotaManagerProxy* quota_manager_proxy)
    : AppCacheServiceImpl(quota_manager_proxy), resource_context_(nullptr) {}

void ChromeAppCacheService::InitializeOnIOThread(
    const base::FilePath& cache_path,
    ResourceContext* resource_context,
    scoped_refptr<net::URLRequestContextGetter> request_context_getter,
    scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  cache_path_ = cache_path;
  resource_context_ = resource_context;

  // The |request_context_getter| can be NULL in some unit tests.
  //
  // TODO(ajwong): TestProfile is difficult to work with. The
  // SafeBrowsing tests require that GetRequestContext return NULL
  // so we can't depend on having a non-NULL value here. See crbug/149783.
  if (request_context_getter)
    set_request_context(request_context_getter->GetURLRequestContext());

  // Init our base class.
  Initialize(cache_path_);
  set_appcache_policy(this);
  set_special_storage_policy(special_storage_policy.get());
}

void ChromeAppCacheService::CreateBackend(
    int process_id,
    mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver) {
  // The process_id is the id of the RenderProcessHost, which can be reused for
  // a new renderer process if the previous renderer process was shutdown.
  // It can take some time after shutdown for the pipe error to propagate
  // and unregister the previous backend. Since the AppCacheService assumes
  // that there is one backend per process_id, we need to ensure that the
  // previous backend is unregistered by eagerly unbinding the pipe.
  Unbind(process_id);

  Bind(std::make_unique<AppCacheBackendImpl>(this, process_id),
       std::move(receiver), process_id);
}

void ChromeAppCacheService::CreateBackendForRequest(
    int process_id,
    blink::mojom::AppCacheBackendRequest request) {
  // Implicit conversion to mojo::PendingReceiver<T>.
  CreateBackend(process_id, std::move(request));
}

void ChromeAppCacheService::Bind(
    std::unique_ptr<blink::mojom::AppCacheBackend> backend,
    mojo::PendingReceiver<blink::mojom::AppCacheBackend> receiver,
    int process_id) {
  DCHECK(process_receivers_.find(process_id) == process_receivers_.end());
  process_receivers_[process_id] =
      receivers_.Add(std::move(backend), std::move(receiver));
}

void ChromeAppCacheService::Unbind(int process_id) {
  auto it = process_receivers_.find(process_id);
  if (it != process_receivers_.end()) {
    receivers_.Remove(it->second);
    DCHECK(process_receivers_.find(process_id) == process_receivers_.end());
  }
}

void ChromeAppCacheService::UnregisterBackend(
    AppCacheBackendImpl* backend_impl) {
  int process_id = backend_impl->process_id();
  process_receivers_.erase(process_receivers_.find(process_id));
  AppCacheServiceImpl::UnregisterBackend(backend_impl);
}

void ChromeAppCacheService::Shutdown() {
  receivers_.Clear();
}

bool ChromeAppCacheService::CanLoadAppCache(const GURL& manifest_url,
                                            const GURL& first_party) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // We don't prompt for read access.
  return GetContentClient()->browser()->AllowAppCache(
      manifest_url, first_party, resource_context_);
}

bool ChromeAppCacheService::CanCreateAppCache(
    const GURL& manifest_url, const GURL& first_party) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return GetContentClient()->browser()->AllowAppCache(
      manifest_url, first_party, resource_context_);
}

ChromeAppCacheService::~ChromeAppCacheService() {}

void ChromeAppCacheService::DeleteOnCorrectThread() const {
  if (BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    delete this;
    return;
  }
  if (BrowserThread::IsThreadInitialized(BrowserThread::IO)) {
    BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE, this);
    return;
  }
  // Better to leak than crash on shutdown.
}

}  // namespace content
