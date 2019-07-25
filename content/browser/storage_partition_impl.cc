// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <set>
#include <utility>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/syslog_logging.h"
#include "base/task/post_task.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "components/variations/net/variations_http_headers.h"
#include "content/browser/background_fetch/background_fetch_context.h"
#include "content/browser/blob_storage/blob_registry_wrapper.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/browsing_data/clear_site_data_handler.h"
#include "content/browser/browsing_data/storage_partition_code_cache_data_remover.h"
#include "content/browser/browsing_data/storage_partition_http_cache_data_remover.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/code_cache/generated_code_cache_context.h"
#include "content/browser/cookie_store/cookie_store_context.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/gpu/shader_cache_factory.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/loader/prefetch_url_loader_service.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/cors_exempt_headers.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/native_file_system_entry_factory.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/browser/storage_usage_info.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "net/base/net_errors.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_util.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ppapi/buildflags/buildflags.h"
#include "services/network/cookie_manager.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/cross_thread_shared_url_loader_factory_info.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/connector.h"
#include "storage/browser/blob/blob_registry_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/quota/quota_manager.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom.h"

#if !defined(OS_ANDROID)
#include "content/browser/host_zoom_map_impl.h"
#endif  // !defined(OS_ANDROID)

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_private_storage_helper.h"
#endif  // BUILDFLAG(ENABLE_PLUGINS)

using CookieDeletionFilter = network::mojom::CookieDeletionFilter;
using CookieDeletionFilterPtr = network::mojom::CookieDeletionFilterPtr;

namespace content {

namespace {

base::LazyInstance<StoragePartitionImpl::CreateNetworkFactoryCallback>::Leaky
    g_url_loader_factory_callback_for_test = LAZY_INSTANCE_INITIALIZER;

void OnClearedCookies(base::OnceClosure callback, uint32_t num_deleted) {
  // The final callback needs to happen from UI thread.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&OnClearedCookies, std::move(callback), num_deleted));
    return;
  }

  std::move(callback).Run();
}

void CheckQuotaManagedDataDeletionStatus(size_t* deletion_task_count,
                                         base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (*deletion_task_count == 0) {
    delete deletion_task_count;
    std::move(callback).Run();
  }
}

void OnQuotaManagedOriginDeleted(const url::Origin& origin,
                                 blink::mojom::StorageType type,
                                 size_t* deletion_task_count,
                                 base::OnceClosure callback,
                                 blink::mojom::QuotaStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(*deletion_task_count, 0u);
  if (status != blink::mojom::QuotaStatusCode::kOk) {
    DLOG(ERROR) << "Couldn't remove data of type " << static_cast<int>(type)
                << " for origin " << origin
                << ". Status: " << static_cast<int>(status);
  }

  (*deletion_task_count)--;
  CheckQuotaManagedDataDeletionStatus(deletion_task_count, std::move(callback));
}

void PerformQuotaManagerStorageCleanup(
    const scoped_refptr<storage::QuotaManager>& quota_manager,
    blink::mojom::StorageType quota_storage_type,
    uint32_t remove_mask,
    base::OnceClosure callback) {
  quota_manager->PerformStorageCleanup(quota_storage_type, remove_mask,
                                       std::move(callback));
}

void ClearedShaderCache(base::OnceClosure callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&ClearedShaderCache, std::move(callback)));
    return;
  }
  std::move(callback).Run();
}

void ClearShaderCacheOnIOThread(const base::FilePath& path,
                                const base::Time begin,
                                const base::Time end,
                                base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetShaderCacheFactorySingleton()->ClearByPath(
      path, begin, end,
      base::BindOnce(&ClearedShaderCache, std::move(callback)));
}

void OnLocalStorageUsageInfo(
    const scoped_refptr<DOMStorageContextWrapper>& dom_storage_context,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    const StoragePartition::OriginMatcherFunction& origin_matcher,
    bool perform_storage_cleanup,
    const base::Time delete_begin,
    const base::Time delete_end,
    base::OnceClosure callback,
    const std::vector<StorageUsageInfo>& infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::OnceClosure done_callback =
      perform_storage_cleanup
          ? base::BindOnce(
                &DOMStorageContextWrapper::PerformLocalStorageCleanup,
                dom_storage_context, std::move(callback))
          : std::move(callback);

  base::RepeatingClosure barrier =
      base::BarrierClosure(infos.size(), std::move(done_callback));
  for (size_t i = 0; i < infos.size(); ++i) {
    if (!origin_matcher.is_null() &&
        !origin_matcher.Run(infos[i].origin, special_storage_policy.get())) {
      barrier.Run();
      continue;
    }

    if (infos[i].last_modified >= delete_begin &&
        infos[i].last_modified <= delete_end) {
      dom_storage_context->DeleteLocalStorage(infos[i].origin, barrier);
    } else {
      barrier.Run();
    }
  }
}

void OnSessionStorageUsageInfo(
    const scoped_refptr<DOMStorageContextWrapper>& dom_storage_context,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    const StoragePartition::OriginMatcherFunction& origin_matcher,
    bool perform_storage_cleanup,
    base::OnceClosure callback,
    const std::vector<SessionStorageUsageInfo>& infos) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::OnceClosure done_callback =
      perform_storage_cleanup
          ? base::BindOnce(
                &DOMStorageContextWrapper::PerformSessionStorageCleanup,
                dom_storage_context, std::move(callback))
          : std::move(callback);

  base::RepeatingClosure barrier =
      base::BarrierClosure(infos.size(), std::move(done_callback));

  for (size_t i = 0; i < infos.size(); ++i) {
    if (!origin_matcher.is_null() &&
        !origin_matcher.Run(url::Origin::Create(infos[i].origin),
                            special_storage_policy.get())) {
      barrier.Run();
      continue;
    }
    dom_storage_context->DeleteSessionStorage(infos[i], barrier);
  }
}

void ClearLocalStorageOnUIThread(
    const scoped_refptr<DOMStorageContextWrapper>& dom_storage_context,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    const StoragePartition::OriginMatcherFunction& origin_matcher,
    const GURL& storage_origin,
    bool perform_storage_cleanup,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!storage_origin.is_empty()) {
    bool can_delete = origin_matcher.is_null() ||
                      origin_matcher.Run(url::Origin::Create(storage_origin),
                                         special_storage_policy.get());
    if (can_delete) {
      dom_storage_context->DeleteLocalStorage(
          url::Origin::Create(storage_origin), std::move(callback));
    } else {
      std::move(callback).Run();
    }
    return;
  }

  dom_storage_context->GetLocalStorageUsage(
      base::BindOnce(&OnLocalStorageUsageInfo, dom_storage_context,
                     special_storage_policy, origin_matcher,
                     perform_storage_cleanup, begin, end, std::move(callback)));
}

void ClearSessionStorageOnUIThread(
    const scoped_refptr<DOMStorageContextWrapper>& dom_storage_context,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    const StoragePartition::OriginMatcherFunction& origin_matcher,
    bool perform_storage_cleanup,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  dom_storage_context->GetSessionStorageUsage(base::BindOnce(
      &OnSessionStorageUsageInfo, dom_storage_context, special_storage_policy,
      origin_matcher, perform_storage_cleanup, std::move(callback)));
}

WebContents* GetWebContentsForStoragePartition(uint32_t process_id,
                                               uint32_t routing_id) {
  if (process_id != network::mojom::kBrowserProcessId) {
    return WebContentsImpl::FromRenderFrameHostID(process_id, routing_id);
  }
  return WebContents::FromFrameTreeNodeId(routing_id);
}

BrowserContext* GetBrowserContextFromStoragePartition(
    base::WeakPtr<StoragePartitionImpl> weak_partition_ptr) {
  return weak_partition_ptr ? weak_partition_ptr->browser_context() : nullptr;
}

// TODO(crbug.com/977040): Remove when no longer needed.
void DeprecateSameSiteCookies(int process_id,
                              int routing_id,
                              const net::CookieStatusList& cookie_list) {
  // Navigation requests start in the browser, before process_id is assigned, so
  // the id is set to network::mojom::kBrowserProcessId. In these situations,
  // the routing_id is the frame tree node id, and can be used directly.
  RenderFrameHostImpl* frame = nullptr;
  if (process_id == network::mojom::kBrowserProcessId) {
    FrameTreeNode* ftn = FrameTreeNode::GloballyFindByID(routing_id);
    if (ftn)
      frame = ftn->current_frame_host();
  } else {
    frame = RenderFrameHostImpl::FromID(process_id, routing_id);
  }

  if (!frame)
    return;

  // Because of the nature of mojo and calling cross process, there's the
  // possibility of calling this method after the page has already been
  // navigated away from, which is DCHECKed against in
  // LogWebFeatureForCurrentPage. We're replicating the DCHECK here and
  // returning early should this be the case.
  WebContents* web_contents = WebContents::FromRenderFrameHost(frame);

  // |web_contents| will be null on interstitial pages, which means the frame
  // has been navigated away from and the function should return early.
  if (!web_contents)
    return;

  RenderFrameHostImpl* root_frame_host = frame;
  while (root_frame_host->GetParent() != nullptr)
    root_frame_host = root_frame_host->GetParent();

  if (root_frame_host != web_contents->GetMainFrame())
    return;

  bool samesite_treated_as_lax_cookies = false;
  bool samesite_none_insecure_cookies = false;

  bool emit_messages =
      base::FeatureList::IsEnabled(features::kCookieDeprecationMessages);

  for (const net::CookieWithStatus& excluded_cookie : cookie_list) {
    std::string cookie_url =
        net::cookie_util::CookieOriginToURL(excluded_cookie.cookie.Domain(),
                                            excluded_cookie.cookie.IsSecure())
            .possibly_invalid_spec();

    if (excluded_cookie.status ==
        net::CanonicalCookie::CookieInclusionStatus::
            EXCLUDE_SAMESITE_UNSPECIFIED_TREATED_AS_LAX) {
      samesite_treated_as_lax_cookies = true;
    }
    if (excluded_cookie.status == net::CanonicalCookie::CookieInclusionStatus::
                                      EXCLUDE_SAMESITE_NONE_INSECURE) {
      samesite_none_insecure_cookies = true;
    }
    if (emit_messages) {
      root_frame_host->AddSameSiteCookieDeprecationMessage(
          cookie_url, excluded_cookie.status);
    }
  }

  if (samesite_treated_as_lax_cookies) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        frame, blink::mojom::WebFeature::kCookieNoSameSite);
  }

  if (samesite_none_insecure_cookies) {
    GetContentClient()->browser()->LogWebFeatureForCurrentPage(
        frame, blink::mojom::WebFeature::kCookieInsecureAndSameSiteNone);
  }
}

void ReportCookiesChangedOnUI(
    std::vector<GlobalFrameRoutingId> destinations,
    const GURL& url,
    const GURL& site_for_cookies,
    const std::vector<net::CookieWithStatus>& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (const GlobalFrameRoutingId& id : destinations) {
    DeprecateSameSiteCookies(id.child_id, id.frame_routing_id, cookie_list);
  }

  for (const auto& cookie_and_status : cookie_list) {
    switch (cookie_and_status.status) {
      case net::CanonicalCookie::CookieInclusionStatus::
          EXCLUDE_USER_PREFERENCES:
        for (const GlobalFrameRoutingId& id : destinations) {
          WebContents* web_contents = GetWebContentsForStoragePartition(
              id.child_id, id.frame_routing_id);
          if (!web_contents)
            continue;
          web_contents->OnCookieChange(url, site_for_cookies,
                                       cookie_and_status.cookie,
                                       /* blocked_by_policy =*/true);
        }
        break;
      case net::CanonicalCookie::CookieInclusionStatus::INCLUDE:
        for (const GlobalFrameRoutingId& id : destinations) {
          WebContents* web_contents = GetWebContentsForStoragePartition(
              id.child_id, id.frame_routing_id);
          if (!web_contents)
            continue;
          web_contents->OnCookieChange(url, site_for_cookies,
                                       cookie_and_status.cookie,
                                       /* blocked_by_policy =*/false);
        }
        break;
      default:
        break;
    }
  }
}

void ReportCookiesReadOnUI(
    std::vector<GlobalFrameRoutingId> destinations,
    const GURL& url,
    const GURL& site_for_cookies,
    const std::vector<net::CookieWithStatus>& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  for (const GlobalFrameRoutingId& id : destinations) {
    DeprecateSameSiteCookies(id.child_id, id.frame_routing_id, cookie_list);
  }

  net::CookieList accepted, blocked;
  for (auto& cookie_and_status : cookie_list) {
    switch (cookie_and_status.status) {
      case net::CanonicalCookie::CookieInclusionStatus::
          EXCLUDE_USER_PREFERENCES:
        blocked.push_back(std::move(cookie_and_status.cookie));
        break;
      case net::CanonicalCookie::CookieInclusionStatus::INCLUDE:
        accepted.push_back(std::move(cookie_and_status.cookie));
        break;
      default:
        break;
    }
  }

  if (!accepted.empty()) {
    for (const GlobalFrameRoutingId& id : destinations) {
      WebContents* web_contents =
          GetWebContentsForStoragePartition(id.child_id, id.frame_routing_id);
      if (!web_contents)
        continue;
      web_contents->OnCookiesRead(url, site_for_cookies, accepted,
                                  /* blocked_by_policy =*/false);
    }
  }

  if (!blocked.empty()) {
    for (const GlobalFrameRoutingId& id : destinations) {
      WebContents* web_contents =
          GetWebContentsForStoragePartition(id.child_id, id.frame_routing_id);
      if (!web_contents)
        continue;
      web_contents->OnCookiesRead(url, site_for_cookies, blocked,
                                  /* blocked_by_policy =*/true);
    }
  }
}

void OnServiceWorkerCookiesReadOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    const GURL& url,
    const GURL& site_for_cookies,
    const std::vector<net::CookieWithStatus>& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Notify all the frames associated with this service worker of its cookie
  // activity.
  std::unique_ptr<std::vector<GlobalFrameRoutingId>> host_ids =
      service_worker_context->GetProviderHostIds(url.GetOrigin());
  if (!host_ids->empty()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(ReportCookiesReadOnUI, *host_ids, url, site_for_cookies,
                       cookie_list));
  }
}

void OnServiceWorkerCookiesChangedOnIO(
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    const GURL& url,
    const GURL& site_for_cookies,
    const std::vector<net::CookieWithStatus>& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Notify all the frames associated with this service worker of its cookie
  // activity.
  std::unique_ptr<std::vector<GlobalFrameRoutingId>> host_ids =
      service_worker_context->GetProviderHostIds(url.GetOrigin());
  if (!host_ids->empty()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(ReportCookiesChangedOnUI, *host_ids, url,
                       site_for_cookies, cookie_list));
  }
}

}  // namespace

// Class to own the NetworkContext wrapping a storage partitions
// URLRequestContext, when the ContentBrowserClient doesn't provide a
// NetworkContext itself.
//
// Created on the UI thread, but must be initialized and destroyed on the IO
// thread.
class StoragePartitionImpl::NetworkContextOwner {
 public:
  NetworkContextOwner() { DCHECK_CURRENTLY_ON(BrowserThread::UI); }

  ~NetworkContextOwner() { DCHECK_CURRENTLY_ON(BrowserThread::IO); }

  void Initialize(network::mojom::NetworkContextRequest network_context_request,
                  scoped_refptr<net::URLRequestContextGetter> context_getter) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    network::mojom::NetworkContextParamsPtr network_context_params =
        network::mojom::NetworkContextParams::New();
    content::UpdateCorsExemptHeader(network_context_params.get());
    variations::UpdateCorsExemptHeaderForVariations(

        network_context_params.get());
    context_getter_ = std::move(context_getter);
    network_context_ = std::make_unique<network::NetworkContext>(
        GetNetworkServiceImpl(), std::move(network_context_request),
        context_getter_->GetURLRequestContext(),
        network_context_params->cors_exempt_header_list);
  }

 private:
  // Reference to the URLRequestContextGetter for the URLRequestContext used by
  // NetworkContext. Depending on the embedder's implementation, this may be
  // needed to keep the URLRequestContext alive until the NetworkContext is
  // destroyed.
  scoped_refptr<net::URLRequestContextGetter> context_getter_;
  std::unique_ptr<network::mojom::NetworkContext> network_context_;

  DISALLOW_COPY_AND_ASSIGN(NetworkContextOwner);
};

class StoragePartitionImpl::URLLoaderFactoryForBrowserProcess
    : public network::SharedURLLoaderFactory {
 public:
  explicit URLLoaderFactoryForBrowserProcess(
      StoragePartitionImpl* storage_partition,
      bool corb_enabled)
      : storage_partition_(storage_partition), corb_enabled_(corb_enabled) {}

  // mojom::URLLoaderFactory implementation:

  void CreateLoaderAndStart(network::mojom::URLLoaderRequest request,
                            int32_t routing_id,
                            int32_t request_id,
                            uint32_t options,
                            const network::ResourceRequest& url_request,
                            network::mojom::URLLoaderClientPtr client,
                            const net::MutableNetworkTrafficAnnotationTag&
                                traffic_annotation) override {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (!storage_partition_)
      return;
    storage_partition_
        ->GetURLLoaderFactoryForBrowserProcessInternal(corb_enabled_)
        ->CreateLoaderAndStart(std::move(request), routing_id, request_id,
                               options, url_request, std::move(client),
                               traffic_annotation);
  }

  void Clone(network::mojom::URLLoaderFactoryRequest request) override {
    if (!storage_partition_)
      return;
    storage_partition_
        ->GetURLLoaderFactoryForBrowserProcessInternal(corb_enabled_)
        ->Clone(std::move(request));
  }

  // SharedURLLoaderFactory implementation:
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> Clone() override {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    return std::make_unique<network::CrossThreadSharedURLLoaderFactoryInfo>(
        this);
  }

  void Shutdown() { storage_partition_ = nullptr; }

 private:
  friend class base::RefCounted<URLLoaderFactoryForBrowserProcess>;
  ~URLLoaderFactoryForBrowserProcess() override {}

  StoragePartitionImpl* storage_partition_;
  const bool corb_enabled_;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryForBrowserProcess);
};

// Static.
int StoragePartitionImpl::GenerateQuotaClientMask(uint32_t remove_mask) {
  int quota_client_mask = 0;

  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_FILE_SYSTEMS)
    quota_client_mask |= storage::QuotaClient::kFileSystem;
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_WEBSQL)
    quota_client_mask |= storage::QuotaClient::kDatabase;
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_APPCACHE)
    quota_client_mask |= storage::QuotaClient::kAppcache;
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_INDEXEDDB)
    quota_client_mask |= storage::QuotaClient::kIndexedDatabase;
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_SERVICE_WORKERS)
    quota_client_mask |= storage::QuotaClient::kServiceWorker;
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_CACHE_STORAGE)
    quota_client_mask |= storage::QuotaClient::kServiceWorkerCache;
  if (remove_mask & StoragePartition::REMOVE_DATA_MASK_BACKGROUND_FETCH)
    quota_client_mask |= storage::QuotaClient::kBackgroundFetch;

  return quota_client_mask;
}

// static
void StoragePartitionImpl::
    SetGetURLLoaderFactoryForBrowserProcessCallbackForTesting(
        const CreateNetworkFactoryCallback& url_loader_factory_callback) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(url_loader_factory_callback.is_null() ||
         g_url_loader_factory_callback_for_test.Get().is_null())
      << "It is not expected that this is called with non-null callback when "
      << "another overriding callback is already set.";
  g_url_loader_factory_callback_for_test.Get() = url_loader_factory_callback;
}

// Helper for deleting quota managed data from a partition.
//
// Most of the operations in this class are done on IO thread.
class StoragePartitionImpl::QuotaManagedDataDeletionHelper {
 public:
  QuotaManagedDataDeletionHelper(
      uint32_t remove_mask,
      uint32_t quota_storage_remove_mask,
      const base::Optional<url::Origin>& storage_origin,
      base::OnceClosure callback)
      : remove_mask_(remove_mask),
        quota_storage_remove_mask_(quota_storage_remove_mask),
        storage_origin_(storage_origin),
        callback_(std::move(callback)),
        task_count_(0) {
    DCHECK(!storage_origin_.has_value() ||
           !storage_origin_->GetURL().is_empty());
  }

  void IncrementTaskCountOnIO();
  void DecrementTaskCountOnIO();

  void ClearDataOnIOThread(
      const scoped_refptr<storage::QuotaManager>& quota_manager,
      const base::Time begin,
      const scoped_refptr<storage::SpecialStoragePolicy>&
          special_storage_policy,
      const StoragePartition::OriginMatcherFunction& origin_matcher,
      bool perform_storage_cleanup);

  void ClearOriginsOnIOThread(
      storage::QuotaManager* quota_manager,
      const scoped_refptr<storage::SpecialStoragePolicy>&
          special_storage_policy,
      const StoragePartition::OriginMatcherFunction& origin_matcher,
      bool perform_storage_cleanup,
      base::OnceClosure callback,
      const std::set<url::Origin>& origins,
      blink::mojom::StorageType quota_storage_type);

 private:
  // All of these data are accessed on IO thread.
  uint32_t remove_mask_;
  uint32_t quota_storage_remove_mask_;
  base::Optional<url::Origin> storage_origin_;
  base::OnceClosure callback_;
  int task_count_;

  DISALLOW_COPY_AND_ASSIGN(QuotaManagedDataDeletionHelper);
};

// Helper for deleting all sorts of data from a partition, keeps track of
// deletion status.
//
// StoragePartitionImpl creates an instance of this class to keep track of
// data deletion progress. Deletion requires deleting multiple bits of data
// (e.g. cookies, local storage, session storage etc.) and hopping between UI
// and IO thread. An instance of this class is created in the beginning of
// deletion process (StoragePartitionImpl::ClearDataImpl) and the instance is
// forwarded and updated on each (sub) deletion's callback. The instance is
// finally destroyed when deletion completes (and |callback| is invoked).
class StoragePartitionImpl::DataDeletionHelper {
 public:
  DataDeletionHelper(uint32_t remove_mask,
                     uint32_t quota_storage_remove_mask,
                     base::OnceClosure callback)
      : remove_mask_(remove_mask),
        quota_storage_remove_mask_(quota_storage_remove_mask),
        callback_(std::move(callback)),
        task_count_(0) {}

  ~DataDeletionHelper() {}

  void ClearDataOnUIThread(
      const GURL& storage_origin,
      const OriginMatcherFunction& origin_matcher,
      CookieDeletionFilterPtr cookie_deletion_filter,
      const base::FilePath& path,
      DOMStorageContextWrapper* dom_storage_context,
      storage::QuotaManager* quota_manager,
      storage::SpecialStoragePolicy* special_storage_policy,
      storage::FileSystemContext* filesystem_context,
      network::mojom::CookieManager* cookie_manager,
      bool perform_storage_cleanup,
      const base::Time begin,
      const base::Time end);

  void ClearQuotaManagedDataOnIOThread(
      const scoped_refptr<storage::QuotaManager>& quota_manager,
      const base::Time begin,
      const GURL& storage_origin,
      const scoped_refptr<storage::SpecialStoragePolicy>&
          special_storage_policy,
      const StoragePartition::OriginMatcherFunction& origin_matcher,
      bool perform_storage_cleanup,
      base::OnceClosure callback);

 private:
  enum class TracingDataType {
    kSynchronous = 1,
    kCookies = 2,
    kQuota = 3,
    kLocalStorage = 4,
    kSessionStorage = 5,
    kShaderCache = 6,
    kPluginPrivate = 7,
  };

  base::OnceClosure CreateTaskCompletionClosure(TracingDataType data_type);
  void OnTaskComplete(int tracing_id);  // Callable on any thread.

  uint32_t remove_mask_;
  uint32_t quota_storage_remove_mask_;

  // Accessed on UI thread.
  base::OnceClosure callback_;
  // Accessed on UI thread.
  int task_count_;

  DISALLOW_COPY_AND_ASSIGN(DataDeletionHelper);
};

void StoragePartitionImpl::DataDeletionHelper::ClearQuotaManagedDataOnIOThread(
    const scoped_refptr<storage::QuotaManager>& quota_manager,
    const base::Time begin,
    const GURL& storage_origin,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    const StoragePartition::OriginMatcherFunction& origin_matcher,
    bool perform_storage_cleanup,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  StoragePartitionImpl::QuotaManagedDataDeletionHelper* helper =
      new StoragePartitionImpl::QuotaManagedDataDeletionHelper(
          remove_mask_, quota_storage_remove_mask_,
          storage_origin.is_empty()
              ? base::nullopt
              : base::make_optional(url::Origin::Create(storage_origin)),
          std::move(callback));
  helper->ClearDataOnIOThread(quota_manager, begin, special_storage_policy,
                              origin_matcher, perform_storage_cleanup);
}

StoragePartitionImpl::StoragePartitionImpl(
    BrowserContext* browser_context,
    const base::FilePath& partition_path,
    storage::SpecialStoragePolicy* special_storage_policy)
    : partition_path_(partition_path),
      special_storage_policy_(special_storage_policy),
      network_context_client_binding_(this),
      browser_context_(browser_context),
      deletion_helpers_running_(0) {}

StoragePartitionImpl::~StoragePartitionImpl() {
  browser_context_ = nullptr;

  if (url_loader_factory_getter_)
    url_loader_factory_getter_->OnStoragePartitionDestroyed();

  if (shared_url_loader_factory_for_browser_process_) {
    shared_url_loader_factory_for_browser_process_->Shutdown();
  }
  if (shared_url_loader_factory_for_browser_process_with_corb_) {
    shared_url_loader_factory_for_browser_process_with_corb_->Shutdown();
  }

  if (GetDatabaseTracker()) {
    GetDatabaseTracker()->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&storage::DatabaseTracker::Shutdown,
                                  GetDatabaseTracker()));
  }

  if (GetFileSystemContext())
    GetFileSystemContext()->Shutdown();

  if (GetDOMStorageContext())
    GetDOMStorageContext()->Shutdown();

  if (GetServiceWorkerContext())
    GetServiceWorkerContext()->Shutdown();

  if (GetIndexedDBContext())
    GetIndexedDBContext()->Shutdown();

  if (GetCacheStorageContext())
    GetCacheStorageContext()->Shutdown();

  if (GetPlatformNotificationContext())
    GetPlatformNotificationContext()->Shutdown();

  if (GetBackgroundSyncContext())
    GetBackgroundSyncContext()->Shutdown();

  if (GetPaymentAppContext())
    GetPaymentAppContext()->Shutdown();

  if (GetBackgroundFetchContext())
    GetBackgroundFetchContext()->Shutdown();

  if (GetContentIndexContext())
    GetContentIndexContext()->Shutdown();

  if (GetAppCacheService()) {
    if (NavigationURLLoaderImpl::IsNavigationLoaderOnUIEnabled()) {
      GetAppCacheService()->Shutdown();
    } else {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(&ChromeAppCacheService::Shutdown, appcache_service_));
    }
  }

  BrowserThread::DeleteSoon(BrowserThread::IO, FROM_HERE,
                            std::move(network_context_owner_));
}

// static
std::unique_ptr<StoragePartitionImpl> StoragePartitionImpl::Create(
    BrowserContext* context,
    bool in_memory,
    const base::FilePath& relative_partition_path,
    const std::string& partition_domain) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsThreadInitialized(BrowserThread::UI));

  base::FilePath partition_path =
      context->GetPath().Append(relative_partition_path);

  std::unique_ptr<StoragePartitionImpl> partition =
      base::WrapUnique(new StoragePartitionImpl(
          context, partition_path, context->GetSpecialStoragePolicy()));

  partition->is_in_memory_ = in_memory;
  partition->relative_partition_path_ = relative_partition_path;

  // All of the clients have to be created and registered with the
  // QuotaManager prior to the QuotaManger being used. We do them
  // all together here prior to handing out a reference to anything
  // that utilizes the QuotaManager.
  partition->quota_manager_ = new storage::QuotaManager(
      in_memory, partition_path,
      base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO}).get(),
      context->GetSpecialStoragePolicy(),
      base::BindRepeating(&StoragePartitionImpl::GetQuotaSettings,
                          partition->weak_factory_.GetWeakPtr()));
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy =
      partition->quota_manager_->proxy();

  // Each consumer is responsible for registering its QuotaClient during
  // its construction.
  partition->filesystem_context_ = CreateFileSystemContext(
      context, partition_path, in_memory, quota_manager_proxy.get());

  partition->database_tracker_ = base::MakeRefCounted<storage::DatabaseTracker>(
      partition_path, in_memory, context->GetSpecialStoragePolicy(),
      quota_manager_proxy.get());

  partition->dom_storage_context_ = DOMStorageContextWrapper::Create(
      BrowserContext::GetConnectorFor(context),
      in_memory ? base::FilePath() : context->GetPath(),
      relative_partition_path, context->GetSpecialStoragePolicy());

  partition->idle_manager_ = std::make_unique<IdleManager>();
  partition->lock_manager_ = new LockManager();

  base::FilePath path = in_memory ? base::FilePath() : partition_path;
  partition->indexed_db_context_ = new IndexedDBContextImpl(
      path, context->GetSpecialStoragePolicy(), quota_manager_proxy,
      base::DefaultClock::GetInstance());

  partition->cache_storage_context_ = new CacheStorageContextImpl(context);
  partition->cache_storage_context_->Init(
      path, context->GetSpecialStoragePolicy(), quota_manager_proxy);

  partition->service_worker_context_ = new ServiceWorkerContextWrapper(context);
  partition->service_worker_context_->set_storage_partition(partition.get());

  partition->appcache_service_ = base::MakeRefCounted<ChromeAppCacheService>(
      quota_manager_proxy.get(), partition->weak_factory_.GetWeakPtr());

  partition->shared_worker_service_ = std::make_unique<SharedWorkerServiceImpl>(
      partition.get(), partition->service_worker_context_,
      partition->appcache_service_);

  partition->push_messaging_context_ =
      new PushMessagingContext(context, partition->service_worker_context_);

#if !defined(OS_ANDROID)
  partition->host_zoom_level_context_ = new HostZoomLevelContext(
      context->CreateZoomLevelDelegate(partition_path));
#endif  // !defined(OS_ANDROID)

  partition->platform_notification_context_ =
      new PlatformNotificationContextImpl(path, context,
                                          partition->service_worker_context_);
  partition->platform_notification_context_->Initialize();

  partition->devtools_background_services_context_ =
      base::MakeRefCounted<DevToolsBackgroundServicesContextImpl>(
          context, partition->service_worker_context_);

  partition->content_index_context_ =
      base::MakeRefCounted<ContentIndexContextImpl>(
          context, partition->service_worker_context_);

  partition->background_fetch_context_ =
      base::MakeRefCounted<BackgroundFetchContext>(
          context, partition->service_worker_context_,
          partition->cache_storage_context_, quota_manager_proxy,
          partition->devtools_background_services_context_);

  partition->background_sync_context_ =
      base::MakeRefCounted<BackgroundSyncContextImpl>();
  partition->background_sync_context_->Init(
      partition->service_worker_context_,
      partition->devtools_background_services_context_);

  partition->payment_app_context_ = new PaymentAppContextImpl();
  partition->payment_app_context_->Init(partition->service_worker_context_);

  partition->broadcast_channel_provider_ = new BroadcastChannelProvider();

  partition->bluetooth_allowed_devices_map_ = new BluetoothAllowedDevicesMap();

  scoped_refptr<ChromeBlobStorageContext> blob_context =
      ChromeBlobStorageContext::GetFor(context);

  partition->url_loader_factory_getter_ = new URLLoaderFactoryGetter();
  partition->url_loader_factory_getter_->Initialize(partition.get());

  partition->service_worker_context_->Init(
      path, quota_manager_proxy.get(), context->GetSpecialStoragePolicy(),
      blob_context.get(), partition->url_loader_factory_getter_.get());

  partition->blob_registry_ =
      BlobRegistryWrapper::Create(blob_context, partition->filesystem_context_);

  partition->appcache_service_->set_url_loader_factory_getter(
      partition->url_loader_factory_getter_.get());

  partition->prefetch_url_loader_service_ =
      base::MakeRefCounted<PrefetchURLLoaderService>(context);

  partition->cookie_store_context_ = base::MakeRefCounted<CookieStoreContext>();
  // Unit tests use the Initialize() callback to crash early if restoring the
  // CookieManagerStore's state from ServiceWorkerStorage fails. Production and
  // browser tests rely on CookieStoreManager's well-defined behavior when
  // restoring the state fails.
  partition->cookie_store_context_->Initialize(
      partition->service_worker_context_, base::DoNothing());

  partition->native_file_system_manager_ =
      base::MakeRefCounted<NativeFileSystemManagerImpl>(
          partition->filesystem_context_, blob_context,
          context->GetNativeFileSystemPermissionContext());

  GeneratedCodeCacheSettings settings =
      GetContentClient()->browser()->GetGeneratedCodeCacheSettings(context);

  // For Incognito mode, we should not persist anything on the disk so
  // we do not create a code cache. Caching the generated code in memory
  // is not useful, since V8 already maintains one copy in memory.
  if (!in_memory && settings.enabled()) {
    partition->generated_code_cache_context_ =
        base::MakeRefCounted<GeneratedCodeCacheContext>();

    base::FilePath code_cache_path;
    if (partition_domain.empty()) {
      code_cache_path = settings.path().AppendASCII("Code Cache");
    } else {
      // For site isolated partitions use the config directory.
      code_cache_path = settings.path()
                            .Append(relative_partition_path)
                            .AppendASCII("Code Cache");
    }
    DCHECK_GE(settings.size_in_bytes(), 0);
    partition->GetGeneratedCodeCacheContext()->Initialize(
        code_cache_path, settings.size_in_bytes());
  }

  return partition;
}

base::FilePath StoragePartitionImpl::GetPath() {
  return partition_path_;
}

net::URLRequestContextGetter* StoragePartitionImpl::GetURLRequestContext() {
  DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
  return url_request_context_.get();
}

net::URLRequestContextGetter*
StoragePartitionImpl::GetMediaURLRequestContext() {
  if (base::FeatureList::IsEnabled(network::features::kNetworkService))
    NOTREACHED();
  return media_url_request_context_.get();
}

network::mojom::NetworkContext* StoragePartitionImpl::GetNetworkContext() {
  if (!network_context_.is_bound())
    InitNetworkContext();
  return network_context_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
StoragePartitionImpl::GetURLLoaderFactoryForBrowserProcess() {
  if (!shared_url_loader_factory_for_browser_process_) {
    shared_url_loader_factory_for_browser_process_ =
        new URLLoaderFactoryForBrowserProcess(this, false /* corb_enabled */);
  }
  return shared_url_loader_factory_for_browser_process_;
}

scoped_refptr<network::SharedURLLoaderFactory>
StoragePartitionImpl::GetURLLoaderFactoryForBrowserProcessWithCORBEnabled() {
  if (!shared_url_loader_factory_for_browser_process_with_corb_) {
    shared_url_loader_factory_for_browser_process_with_corb_ =
        new URLLoaderFactoryForBrowserProcess(this, true /* corb_enabled */);
  }
  return shared_url_loader_factory_for_browser_process_with_corb_;
}

std::unique_ptr<network::SharedURLLoaderFactoryInfo>
StoragePartitionImpl::GetURLLoaderFactoryForBrowserProcessIOThread() {
  return url_loader_factory_getter_->GetNetworkFactoryInfo();
}

network::mojom::CookieManager*
StoragePartitionImpl::GetCookieManagerForBrowserProcess() {
  // Create the CookieManager as needed.
  if (!cookie_manager_for_browser_process_ ||
      cookie_manager_for_browser_process_.encountered_error()) {
    GetNetworkContext()->GetCookieManager(
        mojo::MakeRequest(&cookie_manager_for_browser_process_));
  }
  return cookie_manager_for_browser_process_.get();
}

void StoragePartitionImpl::CreateRestrictedCookieManager(
    network::mojom::RestrictedCookieManagerRole role,
    const url::Origin& origin,
    bool is_service_worker,
    int process_id,
    int routing_id,
    network::mojom::RestrictedCookieManagerRequest request) {
  if (!GetContentClient()->browser()->WillCreateRestrictedCookieManager(
          role, browser_context_, origin, is_service_worker, process_id,
          routing_id, &request)) {
    GetNetworkContext()->GetRestrictedCookieManager(std::move(request), role,
                                                    origin, is_service_worker,
                                                    process_id, routing_id);
  }
}

storage::QuotaManager* StoragePartitionImpl::GetQuotaManager() {
  return quota_manager_.get();
}

ChromeAppCacheService* StoragePartitionImpl::GetAppCacheService() {
  return appcache_service_.get();
}

BackgroundSyncContextImpl* StoragePartitionImpl::GetBackgroundSyncContext() {
  return background_sync_context_.get();
}

storage::FileSystemContext* StoragePartitionImpl::GetFileSystemContext() {
  return filesystem_context_.get();
}

storage::DatabaseTracker* StoragePartitionImpl::GetDatabaseTracker() {
  return database_tracker_.get();
}

DOMStorageContextWrapper* StoragePartitionImpl::GetDOMStorageContext() {
  return dom_storage_context_.get();
}

IdleManager* StoragePartitionImpl::GetIdleManager() {
  return idle_manager_.get();
}

LockManager* StoragePartitionImpl::GetLockManager() {
  return lock_manager_.get();
}

IndexedDBContextImpl* StoragePartitionImpl::GetIndexedDBContext() {
  return indexed_db_context_.get();
}

NativeFileSystemEntryFactory*
StoragePartitionImpl::GetNativeFileSystemEntryFactory() {
  return native_file_system_manager_.get();
}

CacheStorageContextImpl* StoragePartitionImpl::GetCacheStorageContext() {
  return cache_storage_context_.get();
}

ServiceWorkerContextWrapper* StoragePartitionImpl::GetServiceWorkerContext() {
  return service_worker_context_.get();
}

SharedWorkerServiceImpl* StoragePartitionImpl::GetSharedWorkerService() {
  return shared_worker_service_.get();
}

#if !defined(OS_ANDROID)
HostZoomMap* StoragePartitionImpl::GetHostZoomMap() {
  DCHECK(host_zoom_level_context_.get());
  return host_zoom_level_context_->GetHostZoomMap();
}

HostZoomLevelContext* StoragePartitionImpl::GetHostZoomLevelContext() {
  return host_zoom_level_context_.get();
}

ZoomLevelDelegate* StoragePartitionImpl::GetZoomLevelDelegate() {
  DCHECK(host_zoom_level_context_.get());
  return host_zoom_level_context_->GetZoomLevelDelegate();
}
#endif  // !defined(OS_ANDROID)

PlatformNotificationContextImpl*
StoragePartitionImpl::GetPlatformNotificationContext() {
  return platform_notification_context_.get();
}

BackgroundFetchContext* StoragePartitionImpl::GetBackgroundFetchContext() {
  return background_fetch_context_.get();
}

PaymentAppContextImpl* StoragePartitionImpl::GetPaymentAppContext() {
  return payment_app_context_.get();
}

BroadcastChannelProvider* StoragePartitionImpl::GetBroadcastChannelProvider() {
  return broadcast_channel_provider_.get();
}

BluetoothAllowedDevicesMap*
StoragePartitionImpl::GetBluetoothAllowedDevicesMap() {
  return bluetooth_allowed_devices_map_.get();
}

BlobRegistryWrapper* StoragePartitionImpl::GetBlobRegistry() {
  return blob_registry_.get();
}

PrefetchURLLoaderService* StoragePartitionImpl::GetPrefetchURLLoaderService() {
  return prefetch_url_loader_service_.get();
}

CookieStoreContext* StoragePartitionImpl::GetCookieStoreContext() {
  return cookie_store_context_.get();
}

GeneratedCodeCacheContext*
StoragePartitionImpl::GetGeneratedCodeCacheContext() {
  return generated_code_cache_context_.get();
}

DevToolsBackgroundServicesContextImpl*
StoragePartitionImpl::GetDevToolsBackgroundServicesContext() {
  return devtools_background_services_context_.get();
}

NativeFileSystemManagerImpl*
StoragePartitionImpl::GetNativeFileSystemManager() {
  return native_file_system_manager_.get();
}

ContentIndexContextImpl* StoragePartitionImpl::GetContentIndexContext() {
  return content_index_context_.get();
}

void StoragePartitionImpl::OpenLocalStorage(
    const url::Origin& origin,
    blink::mojom::StorageAreaRequest request) {
  int process_id = bindings_.dispatch_context();
  // TODO(943887): Replace HasSecurityState() call with something that can
  // preserve security state after process shutdown. The security state check
  // is a temporary solution to avoid crashes when this method is run after the
  // process associated with |process_id| has been destroyed.
  // It temporarily restores the old behavior of always allowing access if the
  // process is gone.
  auto* policy = ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(process_id, origin) &&
      policy->HasSecurityState(process_id)) {
    SYSLOG(WARNING) << "Killing renderer: illegal localStorage request.";
    bindings_.ReportBadMessage("Access denied for localStorage request");
    return;
  }
  dom_storage_context_->OpenLocalStorage(origin, std::move(request));
}

void StoragePartitionImpl::OpenSessionStorage(
    const std::string& namespace_id,
    blink::mojom::SessionStorageNamespaceRequest request) {
  int process_id = bindings_.dispatch_context();
  dom_storage_context_->OpenSessionStorage(process_id, namespace_id,
                                           bindings_.GetBadMessageCallback(),
                                           std::move(request));
}

void StoragePartitionImpl::OnCanSendReportingReports(
    const std::vector<url::Origin>& origins,
    OnCanSendReportingReportsCallback callback) {
  PermissionController* permission_controller =
      BrowserContext::GetPermissionController(browser_context_);
  DCHECK(permission_controller);

  std::vector<url::Origin> origins_out;
  for (auto& origin : origins) {
    GURL origin_url = origin.GetURL();
    bool allowed = permission_controller->GetPermissionStatus(
                       PermissionType::BACKGROUND_SYNC, origin_url,
                       origin_url) == blink::mojom::PermissionStatus::GRANTED;
    if (allowed)
      origins_out.push_back(origin);
  }

  std::move(callback).Run(origins_out);
}

void StoragePartitionImpl::OnCanSendDomainReliabilityUpload(
    const GURL& origin,
    OnCanSendDomainReliabilityUploadCallback callback) {
  PermissionController* permission_controller =
      BrowserContext::GetPermissionController(browser_context_);
  std::move(callback).Run(
      permission_controller->GetPermissionStatus(
          content::PermissionType::BACKGROUND_SYNC, origin, origin) ==
      blink::mojom::PermissionStatus::GRANTED);
}

void StoragePartitionImpl::OnClearSiteData(uint32_t process_id,
                                           int32_t routing_id,
                                           const GURL& url,
                                           const std::string& header_value,
                                           int load_flags,
                                           OnClearSiteDataCallback callback) {
  auto browser_context_getter = base::BindRepeating(
      GetBrowserContextFromStoragePartition, weak_factory_.GetWeakPtr());
  auto web_contents_getter = base::BindRepeating(
      GetWebContentsForStoragePartition, process_id, routing_id);
  ClearSiteDataHandler::HandleHeader(browser_context_getter,
                                     web_contents_getter, url, header_value,
                                     load_flags, std::move(callback));
}

void StoragePartitionImpl::OnCookiesChanged(
    bool is_service_worker,
    int32_t process_id,
    int32_t routing_id,
    const GURL& url,
    const GURL& site_for_cookies,
    const std::vector<net::CookieWithStatus>& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_service_worker) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&OnServiceWorkerCookiesChangedOnIO,
                       service_worker_context_, url, site_for_cookies,
                       std::move(cookie_list)));
  } else {
    std::vector<GlobalFrameRoutingId> destination;
    destination.emplace_back(process_id, routing_id);
    ReportCookiesChangedOnUI(destination, url, site_for_cookies, cookie_list);
  }
}

void StoragePartitionImpl::OnCookiesRead(
    bool is_service_worker,
    int32_t process_id,
    int32_t routing_id,
    const GURL& url,
    const GURL& site_for_cookies,
    const std::vector<net::CookieWithStatus>& cookie_list) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (is_service_worker) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&OnServiceWorkerCookiesReadOnIO, service_worker_context_,
                       url, site_for_cookies, std::move(cookie_list)));
  } else {
    std::vector<GlobalFrameRoutingId> destination;
    destination.emplace_back(process_id, routing_id);
    ReportCookiesReadOnUI(destination, url, site_for_cookies, cookie_list);
  }
}

void StoragePartitionImpl::ClearDataImpl(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin,
    const OriginMatcherFunction& origin_matcher,
    CookieDeletionFilterPtr cookie_deletion_filter,
    bool perform_storage_cleanup,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DataDeletionHelper* helper = new DataDeletionHelper(
      remove_mask, quota_storage_remove_mask,
      base::BindOnce(&StoragePartitionImpl::DeletionHelperDone,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
  // |helper| deletes itself when done in
  // DataDeletionHelper::DecrementTaskCount().
  deletion_helpers_running_++;
  helper->ClearDataOnUIThread(
      storage_origin, origin_matcher, std::move(cookie_deletion_filter),
      GetPath(), dom_storage_context_.get(), quota_manager_.get(),
      special_storage_policy_.get(), filesystem_context_.get(),
      GetCookieManagerForBrowserProcess(), perform_storage_cleanup, begin, end);
}

void StoragePartitionImpl::DeletionHelperDone(base::OnceClosure callback) {
  std::move(callback).Run();
  deletion_helpers_running_--;
  if (on_deletion_helpers_done_callback_ && deletion_helpers_running_ == 0) {
    // Notify tests that storage partition is done with all deletion tasks.
    std::move(on_deletion_helpers_done_callback_).Run();
  }
}

void StoragePartitionImpl::QuotaManagedDataDeletionHelper::
    IncrementTaskCountOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  ++task_count_;
}

void StoragePartitionImpl::QuotaManagedDataDeletionHelper::
    DecrementTaskCountOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_GT(task_count_, 0);
  --task_count_;
  if (task_count_)
    return;

  std::move(callback_).Run();
  delete this;
}

void StoragePartitionImpl::QuotaManagedDataDeletionHelper::ClearDataOnIOThread(
    const scoped_refptr<storage::QuotaManager>& quota_manager,
    const base::Time begin,
    const scoped_refptr<storage::SpecialStoragePolicy>& special_storage_policy,
    const StoragePartition::OriginMatcherFunction& origin_matcher,
    bool perform_storage_cleanup) {
  IncrementTaskCountOnIO();
  base::RepeatingClosure decrement_callback = base::BindRepeating(
      &QuotaManagedDataDeletionHelper::DecrementTaskCountOnIO,
      base::Unretained(this));

  if (quota_storage_remove_mask_ & QUOTA_MANAGED_STORAGE_MASK_PERSISTENT) {
    IncrementTaskCountOnIO();
    // Ask the QuotaManager for all origins with persistent quota modified
    // within the user-specified timeframe, and deal with the resulting set in
    // ClearQuotaManagedOriginsOnIOThread().
    quota_manager->GetOriginsModifiedSince(
        blink::mojom::StorageType::kPersistent, begin,
        base::BindOnce(&QuotaManagedDataDeletionHelper::ClearOriginsOnIOThread,
                       base::Unretained(this), base::RetainedRef(quota_manager),
                       special_storage_policy, origin_matcher,
                       perform_storage_cleanup, decrement_callback));
  }

  // Do the same for temporary quota.
  if (quota_storage_remove_mask_ & QUOTA_MANAGED_STORAGE_MASK_TEMPORARY) {
    IncrementTaskCountOnIO();
    quota_manager->GetOriginsModifiedSince(
        blink::mojom::StorageType::kTemporary, begin,
        base::BindOnce(&QuotaManagedDataDeletionHelper::ClearOriginsOnIOThread,
                       base::Unretained(this), base::RetainedRef(quota_manager),
                       special_storage_policy, origin_matcher,
                       perform_storage_cleanup, decrement_callback));
  }

  // Do the same for syncable quota.
  if (quota_storage_remove_mask_ & QUOTA_MANAGED_STORAGE_MASK_SYNCABLE) {
    IncrementTaskCountOnIO();
    quota_manager->GetOriginsModifiedSince(
        blink::mojom::StorageType::kSyncable, begin,
        base::BindOnce(&QuotaManagedDataDeletionHelper::ClearOriginsOnIOThread,
                       base::Unretained(this), base::RetainedRef(quota_manager),
                       special_storage_policy, origin_matcher,
                       perform_storage_cleanup, decrement_callback));
  }

  DecrementTaskCountOnIO();
}

void StoragePartitionImpl::QuotaManagedDataDeletionHelper::
    ClearOriginsOnIOThread(
        storage::QuotaManager* quota_manager,
        const scoped_refptr<storage::SpecialStoragePolicy>&
            special_storage_policy,
        const StoragePartition::OriginMatcherFunction& origin_matcher,
        bool perform_storage_cleanup,
        base::OnceClosure callback,
        const std::set<url::Origin>& origins,
        blink::mojom::StorageType quota_storage_type) {
  // The QuotaManager manages all storage other than cookies, LocalStorage,
  // and SessionStorage. This loop wipes out most HTML5 storage for the given
  // origins.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (origins.empty()) {
    std::move(callback).Run();
    return;
  }

  // The logic below (via CheckQuotaManagedDataDeletionStatus) only
  // invokes the callback when all processing is complete.
  base::RepeatingClosure done_callback = base::AdaptCallbackForRepeating(
      perform_storage_cleanup
          ? base::BindOnce(&PerformQuotaManagerStorageCleanup,
                           base::WrapRefCounted(quota_manager),
                           quota_storage_type, remove_mask_,
                           std::move(callback))
          : std::move(callback));

  size_t* deletion_task_count = new size_t(0u);
  (*deletion_task_count)++;
  for (const auto& origin : origins) {
    // TODO(mkwst): Clean this up, it's slow. http://crbug.com/130746
    if (storage_origin_.has_value() && origin != *storage_origin_)
      continue;

    if (!origin_matcher.is_null() &&
        !origin_matcher.Run(origin, special_storage_policy.get())) {
      continue;
    }

    (*deletion_task_count)++;
    quota_manager->DeleteOriginData(
        origin, quota_storage_type,
        StoragePartitionImpl::GenerateQuotaClientMask(remove_mask_),
        base::BindOnce(&OnQuotaManagedOriginDeleted, origin, quota_storage_type,
                       deletion_task_count, done_callback));
  }
  (*deletion_task_count)--;

  CheckQuotaManagedDataDeletionStatus(deletion_task_count, done_callback);
}

base::OnceClosure
StoragePartitionImpl::DataDeletionHelper::CreateTaskCompletionClosure(
    TracingDataType data_type) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ++task_count_;
  static int tracing_id = 0;
  TRACE_EVENT_ASYNC_BEGIN1("browsing_data", "StoragePartitionImpl",
                           ++tracing_id, "data_type",
                           static_cast<int>(data_type));
  return base::BindOnce(
      &StoragePartitionImpl::DataDeletionHelper::OnTaskComplete,
      base::Unretained(this), tracing_id);
}

void StoragePartitionImpl::DataDeletionHelper::OnTaskComplete(int tracing_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&DataDeletionHelper::OnTaskComplete,
                       base::Unretained(this), tracing_id));
    return;
  }
  DCHECK_GT(task_count_, 0);
  --task_count_;
  TRACE_EVENT_ASYNC_END0("browsing_data", "StoragePartitionImpl", tracing_id);

  if (!task_count_) {
    std::move(callback_).Run();
    delete this;
  }
}

void StoragePartitionImpl::DataDeletionHelper::ClearDataOnUIThread(
    const GURL& storage_origin,
    const OriginMatcherFunction& origin_matcher,
    CookieDeletionFilterPtr cookie_deletion_filter,
    const base::FilePath& path,
    DOMStorageContextWrapper* dom_storage_context,
    storage::QuotaManager* quota_manager,
    storage::SpecialStoragePolicy* special_storage_policy,
    storage::FileSystemContext* filesystem_context,
    network::mojom::CookieManager* cookie_manager,
    bool perform_storage_cleanup,
    const base::Time begin,
    const base::Time end) {
  DCHECK_NE(remove_mask_, 0u);
  DCHECK(!callback_.is_null());

  base::ScopedClosureRunner synchronous_clear_operations(
      CreateTaskCompletionClosure(TracingDataType::kSynchronous));

  if (remove_mask_ & REMOVE_DATA_MASK_COOKIES) {
    // The CookieDeletionFilter has a redundant time interval to |begin| and
    // |end|. Ensure that the filter has no time interval specified to help
    // callers detect when they are using the wrong interval values.
    DCHECK(!cookie_deletion_filter->created_after_time.has_value());
    DCHECK(!cookie_deletion_filter->created_before_time.has_value());

    if (!begin.is_null())
      cookie_deletion_filter->created_after_time = begin;
    if (!end.is_null())
      cookie_deletion_filter->created_before_time = end;

    cookie_manager->DeleteCookies(
        std::move(cookie_deletion_filter),
        base::BindOnce(
            &OnClearedCookies,
            // Handle the cookie store being destroyed and the callback thus not
            // being called.
            mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                CreateTaskCompletionClosure(TracingDataType::kCookies))));
  }

  if (remove_mask_ & REMOVE_DATA_MASK_INDEXEDDB ||
      remove_mask_ & REMOVE_DATA_MASK_WEBSQL ||
      remove_mask_ & REMOVE_DATA_MASK_APPCACHE ||
      remove_mask_ & REMOVE_DATA_MASK_FILE_SYSTEMS ||
      remove_mask_ & REMOVE_DATA_MASK_SERVICE_WORKERS ||
      remove_mask_ & REMOVE_DATA_MASK_CACHE_STORAGE) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &DataDeletionHelper::ClearQuotaManagedDataOnIOThread,
            base::Unretained(this), base::WrapRefCounted(quota_manager), begin,
            storage_origin, base::WrapRefCounted(special_storage_policy),
            origin_matcher, perform_storage_cleanup,
            CreateTaskCompletionClosure(TracingDataType::kQuota)));
  }

  if (remove_mask_ & REMOVE_DATA_MASK_LOCAL_STORAGE) {
    ClearLocalStorageOnUIThread(
        base::WrapRefCounted(dom_storage_context),
        base::WrapRefCounted(special_storage_policy), origin_matcher,
        storage_origin, perform_storage_cleanup, begin, end,
        CreateTaskCompletionClosure(TracingDataType::kLocalStorage));

    // ClearDataImpl cannot clear session storage data when a particular origin
    // is specified. Therefore we ignore clearing session storage in this case.
    // TODO(lazyboy): Fix.
    if (storage_origin.is_empty()) {
      // TODO(crbug.com/960325): Sometimes SessionStorage fails to call its
      // callback. Figure out why.
      ClearSessionStorageOnUIThread(
          base::WrapRefCounted(dom_storage_context),
          base::WrapRefCounted(special_storage_policy), origin_matcher,
          perform_storage_cleanup,
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              CreateTaskCompletionClosure(TracingDataType::kSessionStorage)));
    }
  }

  if (remove_mask_ & REMOVE_DATA_MASK_SHADER_CACHE) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &ClearShaderCacheOnIOThread, path, begin, end,
            CreateTaskCompletionClosure(TracingDataType::kShaderCache)));
  }

#if BUILDFLAG(ENABLE_PLUGINS)
  if (remove_mask_ & REMOVE_DATA_MASK_PLUGIN_PRIVATE_DATA) {
    filesystem_context->default_file_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ClearPluginPrivateDataOnFileTaskRunner,
            base::WrapRefCounted(filesystem_context), storage_origin,
            origin_matcher, base::WrapRefCounted(special_storage_policy), begin,
            end,
            base::AdaptCallbackForRepeating(
                CreateTaskCompletionClosure(TracingDataType::kPluginPrivate))));
  }
#endif  // BUILDFLAG(ENABLE_PLUGINS)

}

void StoragePartitionImpl::ClearDataForOrigin(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CookieDeletionFilterPtr deletion_filter = CookieDeletionFilter::New();
  if (!storage_origin.host().empty())
    deletion_filter->host_name = storage_origin.host();
  ClearDataImpl(remove_mask, quota_storage_remove_mask, storage_origin,
                OriginMatcherFunction(), std::move(deletion_filter), false,
                base::Time(), base::Time::Max(), base::DoNothing());
}

void StoragePartitionImpl::ClearData(uint32_t remove_mask,
                                     uint32_t quota_storage_remove_mask,
                                     const GURL& storage_origin,
                                     const base::Time begin,
                                     const base::Time end,
                                     base::OnceClosure callback) {
  CookieDeletionFilterPtr deletion_filter = CookieDeletionFilter::New();
  if (!storage_origin.host().empty())
    deletion_filter->host_name = storage_origin.host();
  bool perform_storage_cleanup =
      begin.is_null() && end.is_max() && storage_origin.is_empty();
  ClearDataImpl(remove_mask, quota_storage_remove_mask, storage_origin,
                OriginMatcherFunction(), std::move(deletion_filter),
                perform_storage_cleanup, begin, end, std::move(callback));
}

void StoragePartitionImpl::ClearData(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const OriginMatcherFunction& origin_matcher,
    network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
    bool perform_storage_cleanup,
    const base::Time begin,
    const base::Time end,
    base::OnceClosure callback) {
  ClearDataImpl(remove_mask, quota_storage_remove_mask, GURL(), origin_matcher,
                std::move(cookie_deletion_filter), perform_storage_cleanup,
                begin, end, std::move(callback));
}

void StoragePartitionImpl::ClearHttpAndMediaCaches(
    const base::Time begin,
    const base::Time end,
    const base::Callback<bool(const GURL&)>& url_matcher,
    base::OnceClosure callback) {
  // StoragePartitionHttpCacheDataRemover deletes itself when it is done.
  if (url_matcher.is_null()) {
    StoragePartitionHttpCacheDataRemover::CreateForRange(this, begin, end)
        ->Remove(std::move(callback));
  } else {
    StoragePartitionHttpCacheDataRemover::CreateForURLsAndRange(
        this, url_matcher, begin, end)
        ->Remove(std::move(callback));
  }
}

void StoragePartitionImpl::ClearCodeCaches(
    const base::Time begin,
    const base::Time end,
    const base::RepeatingCallback<bool(const GURL&)>& url_matcher,
    base::OnceClosure callback) {
  // StoragePartitionCodeCacheDataRemover deletes itself when it is done.
  StoragePartitionCodeCacheDataRemover::Create(this, url_matcher, begin, end)
      ->Remove(std::move(callback));
}

void StoragePartitionImpl::Flush() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (GetDOMStorageContext())
    GetDOMStorageContext()->Flush();
}

void StoragePartitionImpl::ResetURLLoaderFactories() {
  GetNetworkContext()->ResetURLLoaderFactories();
  url_loader_factory_for_browser_process_.reset();
  url_loader_factory_for_browser_process_with_corb_.reset();
  url_loader_factory_getter_->Initialize(this);
}

void StoragePartitionImpl::ClearBluetoothAllowedDevicesMapForTesting() {
  bluetooth_allowed_devices_map_->Clear();
}

void StoragePartitionImpl::FlushNetworkInterfaceForTesting() {
  DCHECK(network_context_);
  network_context_.FlushForTesting();
  if (url_loader_factory_for_browser_process_)
    url_loader_factory_for_browser_process_.FlushForTesting();
  if (url_loader_factory_for_browser_process_with_corb_)
    url_loader_factory_for_browser_process_with_corb_.FlushForTesting();
  if (cookie_manager_for_browser_process_)
    cookie_manager_for_browser_process_.FlushForTesting();
  if (origin_policy_manager_for_browser_process_)
    origin_policy_manager_for_browser_process_.FlushForTesting();
}

void StoragePartitionImpl::WaitForDeletionTasksForTesting() {
  if (deletion_helpers_running_) {
    base::RunLoop loop;
    on_deletion_helpers_done_callback_ = loop.QuitClosure();
    loop.Run();
  }
}

BrowserContext* StoragePartitionImpl::browser_context() const {
  return browser_context_;
}

mojo::BindingId StoragePartitionImpl::Bind(
    int process_id,
    mojo::InterfaceRequest<blink::mojom::StoragePartitionService> request) {
  return bindings_.AddBinding(this, std::move(request), process_id);
}

void StoragePartitionImpl::Unbind(mojo::BindingId binding_id) {
  bindings_.RemoveBinding(binding_id);
}

void StoragePartitionImpl::OverrideQuotaManagerForTesting(
    storage::QuotaManager* quota_manager) {
  quota_manager_ = quota_manager;
}

void StoragePartitionImpl::OverrideSpecialStoragePolicyForTesting(
    storage::SpecialStoragePolicy* special_storage_policy) {
  special_storage_policy_ = special_storage_policy;
}

void StoragePartitionImpl::ShutdownBackgroundSyncContextForTesting() {
  if (GetBackgroundSyncContext())
    GetBackgroundSyncContext()->Shutdown();
}

void StoragePartitionImpl::OverrideBackgroundSyncContextForTesting(
    BackgroundSyncContextImpl* background_sync_context) {
  DCHECK(!GetBackgroundSyncContext() ||
         !GetBackgroundSyncContext()->background_sync_manager());
  background_sync_context_ = background_sync_context;
}

void StoragePartitionImpl::SetURLRequestContext(
    net::URLRequestContextGetter* url_request_context) {
  url_request_context_ = url_request_context;
}

void StoragePartitionImpl::SetMediaURLRequestContext(
    net::URLRequestContextGetter* media_url_request_context) {
  media_url_request_context_ = media_url_request_context;
}

void StoragePartitionImpl::GetQuotaSettings(
    storage::OptionalQuotaSettingsCallback callback) {
  GetContentClient()->browser()->GetQuotaSettings(browser_context_, this,
                                                  std::move(callback));
}

void StoragePartitionImpl::InitNetworkContext() {
  network_context_ = GetContentClient()->browser()->CreateNetworkContext(
      browser_context_, is_in_memory_, relative_partition_path_);
  if (!network_context_) {
    // TODO(mmenke): Remove once https://crbug.com/827928 is fixed.
    CHECK(url_request_context_);

    DCHECK(!base::FeatureList::IsEnabled(network::features::kNetworkService));
    DCHECK(!network_context_owner_);
    network_context_owner_ = std::make_unique<NetworkContextOwner>();
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(&NetworkContextOwner::Initialize,
                       base::Unretained(network_context_owner_.get()),
                       MakeRequest(&network_context_), url_request_context_));
  }
  network::mojom::NetworkContextClientPtr client_ptr;
  network_context_client_binding_.Close();
  network_context_client_binding_.Bind(mojo::MakeRequest(&client_ptr));
  network_context_->SetClient(std::move(client_ptr));
  network_context_.set_connection_error_handler(base::BindOnce(
      &StoragePartitionImpl::InitNetworkContext, weak_factory_.GetWeakPtr()));
}

network::mojom::URLLoaderFactory*
StoragePartitionImpl::GetURLLoaderFactoryForBrowserProcessInternal(
    bool corb_enabled) {
  auto& url_loader_factory =
      corb_enabled ? url_loader_factory_for_browser_process_with_corb_
                   : url_loader_factory_for_browser_process_;
  auto& is_test_url_loader_factory =
      corb_enabled ? is_test_url_loader_factory_for_browser_process_with_corb_
                   : is_test_url_loader_factory_for_browser_process_;

  // Create the URLLoaderFactory as needed, but make sure not to reuse a
  // previously created one if the test override has changed.
  if (url_loader_factory && !url_loader_factory.encountered_error() &&
      is_test_url_loader_factory !=
          g_url_loader_factory_callback_for_test.Get().is_null()) {
    return url_loader_factory.get();
  }

  network::mojom::URLLoaderFactoryParamsPtr params =
      network::mojom::URLLoaderFactoryParams::New();
  params->process_id = network::mojom::kBrowserProcessId;
  params->is_corb_enabled = corb_enabled;
  params->disable_web_security =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableWebSecurity);
  if (g_url_loader_factory_callback_for_test.Get().is_null()) {
    auto request = mojo::MakeRequest(&url_loader_factory);
    GetNetworkContext()->CreateURLLoaderFactory(std::move(request),
                                                std::move(params));
    is_test_url_loader_factory = false;
    return url_loader_factory.get();
  }

  network::mojom::URLLoaderFactoryPtr original_factory;
  GetNetworkContext()->CreateURLLoaderFactory(
      mojo::MakeRequest(&original_factory), std::move(params));
  url_loader_factory = g_url_loader_factory_callback_for_test.Get().Run(
      std::move(original_factory));
  is_test_url_loader_factory = true;
  return url_loader_factory.get();
}

network::mojom::OriginPolicyManager*
StoragePartitionImpl::GetOriginPolicyManagerForBrowserProcess() {
  if (!origin_policy_manager_for_browser_process_ ||
      origin_policy_manager_for_browser_process_.encountered_error()) {
    GetNetworkContext()->GetOriginPolicyManager(
        mojo::MakeRequest(&origin_policy_manager_for_browser_process_));
  }
  return origin_policy_manager_for_browser_process_.get();
}

void StoragePartitionImpl::SetOriginPolicyManagerForBrowserProcessForTesting(
    network::mojom::OriginPolicyManagerPtr test_origin_policy_manager) {
  origin_policy_manager_for_browser_process_ =
      std::move(test_origin_policy_manager);
}

void StoragePartitionImpl::
    ResetOriginPolicyManagerForBrowserProcessForTesting() {
  origin_policy_manager_for_browser_process_ = nullptr;
}

}  // namespace content
