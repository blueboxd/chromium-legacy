// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_script_fetch_initiator.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/post_task.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/data_url_loader_factory.h"
#include "content/browser/file_url_loader_factory.h"
#include "content/browser/fileapi/file_system_url_loader_factory.h"
#include "content/browser/loader/browser_initiated_resource_request.h"
#include "content/browser/loader/navigation_url_loader_impl.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/worker_host/worker_script_fetcher.h"
#include "content/browser/worker_host/worker_script_loader.h"
#include "content/browser/worker_host/worker_script_loader_factory.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/common/content_features.h"
#include "content/public/common/referrer.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "url/origin.h"

namespace content {

namespace {

// Runs |task| on the thread specified by |thread_id| if already on that thread,
// otherwise posts a task to that thread.
void RunOrPostTask(const base::Location& from_here,
                   BrowserThread::ID thread_id,
                   base::OnceClosure task) {
  if (BrowserThread::CurrentlyOn(thread_id)) {
    std::move(task).Run();
    return;
  }

  base::PostTaskWithTraits(from_here, {thread_id}, std::move(task));
}

}  // namespace

// static
void WorkerScriptFetchInitiator::Start(
    int worker_process_id,
    const GURL& script_url,
    const url::Origin& request_initiator,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    ResourceType resource_type,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    ServiceWorkerNavigationHandle* service_worker_handle,
    AppCacheNavigationHandleCore* appcache_handle_core,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_override,
    StoragePartitionImpl* storage_partition,
    const std::string& storage_domain,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
  DCHECK(storage_partition);
  DCHECK(resource_type == ResourceType::kWorker ||
         resource_type == ResourceType::kSharedWorker)
      << static_cast<int>(resource_type);

  BrowserContext* browser_context = storage_partition->browser_context();
  ResourceContext* resource_context =
      browser_context ? browser_context->GetResourceContext() : nullptr;
  if (!browser_context || !resource_context) {
    // The browser is shutting down. Just drop this request.
    return;
  }

  bool constructor_uses_file_url =
      request_initiator.scheme() == url::kFileScheme;

  // TODO(https://crbug.com/987517): Filesystem URL support on shared workers
  // are now broken.
  bool filesystem_url_support = resource_type == ResourceType::kWorker;

  // Set up the factory bundle for non-NetworkService URLs, e.g.,
  // chrome-extension:// URLs. One factory bundle is consumed by the browser
  // for WorkerScriptLoaderFactory, and one is sent to the renderer for
  // subresource loading.
  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
      factory_bundle_for_browser = CreateFactoryBundle(
          worker_process_id, storage_partition, storage_domain,
          constructor_uses_file_url, filesystem_url_support);
  std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
      subresource_loader_factories = CreateFactoryBundle(
          worker_process_id, storage_partition, storage_domain,
          constructor_uses_file_url, filesystem_url_support);

  // Create a resource request for initiating worker script fetch from the
  // browser process.
  std::unique_ptr<network::ResourceRequest> resource_request;

  // Determine the referrer for the worker script request based on the spec.
  // https://w3c.github.io/webappsec-referrer-policy/#determine-requests-referrer
  Referrer sanitized_referrer = Referrer::SanitizeForRequest(
      script_url,
      Referrer(outside_fetch_client_settings_object->outgoing_referrer,
               outside_fetch_client_settings_object->referrer_policy));

  resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = script_url;
  resource_request->site_for_cookies = script_url;
  resource_request->request_initiator = request_initiator;
  resource_request->referrer = sanitized_referrer.url,
  resource_request->referrer_policy = Referrer::ReferrerPolicyForUrlRequest(
      outside_fetch_client_settings_object->referrer_policy);
  resource_request->resource_type = static_cast<int>(resource_type);

  // For a classic worker script request:
  // https://html.spec.whatwg.org/C/#fetch-a-classic-worker-script
  // Step 1: "Let request be a new request whose ..., mode is "same-origin",
  // ..."
  //
  // For a module worker script request:
  // https://html.spec.whatwg.org/C/#fetch-a-single-module-script
  // Step 6: "If destination is "worker" or "sharedworker" and the top-level
  // module fetch flag is set, then set request's mode to "same-origin"."
  resource_request->mode = network::mojom::RequestMode::kSameOrigin;

  // When the credentials mode is "omit", clear |allow_credentials| and set
  // load flags to disable sending credentials according to the comments in
  // CorsURLLoaderFactory::IsSane().
  // TODO(https://crbug.com/799935): Unify |LOAD_DO_NOT_*| into
  // |allow_credentials|.
  resource_request->credentials_mode = credentials_mode;
  if (credentials_mode == network::mojom::CredentialsMode::kOmit) {
    resource_request->allow_credentials = false;
    const auto load_flags_pattern = net::LOAD_DO_NOT_SAVE_COOKIES |
                                    net::LOAD_DO_NOT_SEND_COOKIES |
                                    net::LOAD_DO_NOT_SEND_AUTH_DATA;
    resource_request->load_flags |= load_flags_pattern;
  }

  switch (resource_type) {
    case ResourceType::kWorker:
      resource_request->fetch_request_context_type =
          static_cast<int>(blink::mojom::RequestContextType::WORKER);
      break;
    case ResourceType::kSharedWorker:
      resource_request->fetch_request_context_type =
          static_cast<int>(blink::mojom::RequestContextType::SHARED_WORKER);
      break;
    default:
      NOTREACHED() << static_cast<int>(resource_type);
      break;
  }

  AddAdditionalRequestHeaders(resource_request.get(), browser_context);

  // When navigation on UI is enabled, service worker and appcache work on the
  // UI thread.
  if (NavigationURLLoaderImpl::IsNavigationLoaderOnUIEnabled()) {
    CreateScriptLoaderOnUI(
        worker_process_id, std::move(resource_request), storage_partition,
        std::move(factory_bundle_for_browser),
        std::move(subresource_loader_factories),
        std::move(service_worker_context), service_worker_handle,
        appcache_handle_core, std::move(blob_url_loader_factory),
        std::move(url_loader_factory_override), std::move(callback));
    return;
  }

  // Otherwise, bounce to the IO thread to setup service worker and appcache
  // support in case the request for the worker script will need to be
  // intercepted by them.
  //
  // This passes |resource_context| to the IO thread. |resource_context| will
  // not be destroyed before the task runs, because the shutdown sequence is:
  // 1. (UI thread) StoragePartitionImpl destructs.
  // 2. (IO thread) ResourceContext destructs.
  // Since |storage_partition| is alive, we must be before step 1, so this
  // task we post to the IO thread must run before step 2.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &WorkerScriptFetchInitiator::CreateScriptLoaderOnIO,
          worker_process_id, std::move(resource_request),
          storage_partition->url_loader_factory_getter(),
          std::move(factory_bundle_for_browser),
          std::move(subresource_loader_factories), resource_context,
          std::move(service_worker_context), service_worker_handle->core(),
          appcache_handle_core,
          blob_url_loader_factory ? blob_url_loader_factory->Clone() : nullptr,
          url_loader_factory_override ? url_loader_factory_override->Clone()
                                      : nullptr,
          std::move(callback)));
}

BrowserThread::ID WorkerScriptFetchInitiator::GetLoaderThreadID() {
  return NavigationURLLoaderImpl::IsNavigationLoaderOnUIEnabled()
             ? BrowserThread::UI
             : BrowserThread::IO;
}

std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
WorkerScriptFetchInitiator::CreateFactoryBundle(
    int worker_process_id,
    StoragePartitionImpl* storage_partition,
    const std::string& storage_domain,
    bool file_support,
    bool filesystem_url_support) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ContentBrowserClient::NonNetworkURLLoaderFactoryMap non_network_factories;
  non_network_factories[url::kDataScheme] =
      std::make_unique<DataURLLoaderFactory>();
  if (filesystem_url_support) {
    // TODO(https://crbug.com/986188): Pass ChildProcessHost::kInvalidUniqueID
    // instead of valid |worker_process_id| for |factory_bundle_for_browser|
    // once CanCommitURL-like check is implemented in PlzWorker.
    non_network_factories[url::kFileSystemScheme] =
        CreateFileSystemURLLoaderFactory(
            worker_process_id, RenderFrameHost::kNoFrameTreeNodeId,
            storage_partition->GetFileSystemContext(), storage_domain);
  }
  GetContentClient()
      ->browser()
      ->RegisterNonNetworkSubresourceURLLoaderFactories(
          worker_process_id, MSG_ROUTING_NONE, &non_network_factories);

  auto factory_bundle = std::make_unique<blink::URLLoaderFactoryBundleInfo>();
  for (auto& pair : non_network_factories) {
    const std::string& scheme = pair.first;
    std::unique_ptr<network::mojom::URLLoaderFactory> factory =
        std::move(pair.second);

    network::mojom::URLLoaderFactoryPtr factory_ptr;
    mojo::MakeStrongBinding(std::move(factory),
                            mojo::MakeRequest(&factory_ptr));
    factory_bundle->pending_scheme_specific_factories().emplace(
        scheme, factory_ptr.PassInterface());
  }

  if (file_support) {
    auto file_factory = std::make_unique<FileURLLoaderFactory>(
        storage_partition->browser_context()->GetPath(),
        storage_partition->browser_context()->GetSharedCorsOriginAccessList(),
        // USER_VISIBLE because worker script fetch may affect the UI.
        base::TaskPriority::USER_VISIBLE);
    network::mojom::URLLoaderFactoryPtr file_factory_ptr;
    mojo::MakeStrongBinding(std::move(file_factory),
                            mojo::MakeRequest(&file_factory_ptr));
    factory_bundle->pending_scheme_specific_factories().emplace(
        url::kFileScheme, file_factory_ptr.PassInterface());
  }

  return factory_bundle;
}

// TODO(nhiroki): Align this function with AddAdditionalRequestHeaders() in
// navigation_request.cc, FrameFetchContext, and WorkerFetchContext.
void WorkerScriptFetchInitiator::AddAdditionalRequestHeaders(
    network::ResourceRequest* resource_request,
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(nhiroki): Return early when the request is neither HTTP nor HTTPS
  // (i.e., Blob URL or Data URL). This should be checked by
  // SchemeIsHTTPOrHTTPS(), but currently cross-origin workers on extensions
  // are allowed and the check doesn't work well. See https://crbug.com/867302.

  // Set the "Accept" header.
  resource_request->headers.SetHeaderIfMissing(network::kAcceptHeader,
                                               network::kDefaultAcceptHeader);

  blink::mojom::RendererPreferences renderer_preferences;
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      browser_context, &renderer_preferences);
  UpdateAdditionalHeadersForBrowserInitiatedRequest(
      &resource_request->headers, browser_context,
      /*should_update_existing_headers=*/false, renderer_preferences);

  SetFetchMetadataHeadersForBrowserInitiatedRequest(resource_request);
}

void WorkerScriptFetchInitiator::CreateScriptLoaderOnUI(
    int worker_process_id,
    std::unique_ptr<network::ResourceRequest> resource_request,
    StoragePartitionImpl* storage_partition,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        factory_bundle_for_browser_info,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    ServiceWorkerNavigationHandle* service_worker_handle,
    AppCacheNavigationHandleCore* appcache_handle_core,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_override,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Create the URL loader factory for WorkerScriptLoaderFactory to use to load
  // the main script.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (blob_url_loader_factory) {
    // If we have a blob_url_loader_factory just use that directly rather than
    // creating a new URLLoaderFactoryBundle.
    url_loader_factory = std::move(blob_url_loader_factory);
  } else if (url_loader_factory_override) {
    // For unit tests.
    url_loader_factory = std::move(url_loader_factory_override);
  } else {
    // Add the default factory to the bundle for browser.
    DCHECK(factory_bundle_for_browser_info);

    // Get the direct network factory. This doesn't support reconnection to the
    // network service after a crash, but it's OK since it's used only for a
    // single request to fetch the worker's main script during startup. If the
    // network service crashes, worker startup should simply fail.
    network::mojom::URLLoaderFactoryPtr network_factory_ptr;
    auto network_factory =
        storage_partition->GetURLLoaderFactoryForBrowserProcess();
    network_factory->Clone(mojo::MakeRequest(&network_factory_ptr));

    factory_bundle_for_browser_info->pending_default_factory() =
        network_factory_ptr.PassInterface();
    url_loader_factory = base::MakeRefCounted<blink::URLLoaderFactoryBundle>(
        std::move(factory_bundle_for_browser_info));
  }

  base::WeakPtr<AppCacheHost> appcache_host =
      appcache_handle_core ? appcache_handle_core->host()->GetWeakPtr()
                           : nullptr;

  // Start loading a web worker main script.
  // TODO(nhiroki): Figure out what we should do in |wc_getter| for loading web
  // worker's main script. Returning the WebContents of the closest ancestor's
  // frame is a possible option, but it doesn't work when a shared worker
  // creates a dedicated worker after the closest ancestor's frame is gone. The
  // frame tree node ID has the same issue.
  base::RepeatingCallback<WebContents*()> wc_getter =
      base::BindRepeating([]() -> WebContents* { return nullptr; });
  std::vector<std::unique_ptr<URLLoaderThrottle>> throttles =
      GetContentClient()->browser()->CreateURLLoaderThrottles(
          *resource_request, storage_partition->browser_context(), wc_getter,
          nullptr /* navigation_ui_data */,
          RenderFrameHost::kNoFrameTreeNodeId);

  // Create a BrowserContext getter using |service_worker_context|.
  // This context is aware of shutdown and safely returns a nullptr
  // instead of a destroyed BrowserContext in that case.
  auto browser_context_getter =
      base::BindRepeating(&ServiceWorkerContextWrapper::browser_context,
                          std::move(service_worker_context));

  WorkerScriptFetcher::CreateAndStart(
      std::make_unique<WorkerScriptLoaderFactory>(
          worker_process_id, service_worker_handle,
          /*service_worker_handle_core=*/nullptr, std::move(appcache_host),
          browser_context_getter,
          base::RepeatingCallback<ResourceContext*(void)>(),
          std::move(url_loader_factory)),
      std::move(throttles), std::move(resource_request),
      base::BindOnce(WorkerScriptFetchInitiator::DidCreateScriptLoader,
                     std::move(callback),
                     std::move(subresource_loader_factories)));
}

void WorkerScriptFetchInitiator::CreateScriptLoaderOnIO(
    int worker_process_id,
    std::unique_ptr<network::ResourceRequest> resource_request,
    scoped_refptr<URLLoaderFactoryGetter> loader_factory_getter,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        factory_bundle_for_browser_info,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    ResourceContext* resource_context,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    ServiceWorkerNavigationHandleCore* service_worker_handle_core,
    AppCacheNavigationHandleCore* appcache_handle_core,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        blob_url_loader_factory_info,
    std::unique_ptr<network::SharedURLLoaderFactoryInfo>
        url_loader_factory_override_info,
    CompletionCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(resource_context);
  DCHECK(service_worker_handle_core);

  auto resource_type =
      static_cast<ResourceType>(resource_request->resource_type);
  auto provider_type = blink::mojom::ServiceWorkerProviderType::kUnknown;
  switch (resource_type) {
    case ResourceType::kWorker:
      provider_type =
          blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker;
      break;
    case ResourceType::kSharedWorker:
      provider_type = blink::mojom::ServiceWorkerProviderType::kForSharedWorker;
      break;
    default:
      NOTREACHED() << resource_request->resource_type;
      break;
  }

  // Create the URL loader factory for WorkerScriptLoaderFactory to use to load
  // the main script.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory;
  if (blob_url_loader_factory_info) {
    // If we have a blob_url_loader_factory just use that directly rather than
    // creating a new URLLoaderFactoryBundle.
    url_loader_factory = network::SharedURLLoaderFactory::Create(
        std::move(blob_url_loader_factory_info));
  } else if (url_loader_factory_override_info) {
    // For unit tests.
    url_loader_factory = network::SharedURLLoaderFactory::Create(
        std::move(url_loader_factory_override_info));
  } else {
    // Add the default factory to the bundle for browser.
    DCHECK(factory_bundle_for_browser_info);

    // Get the direct network factory from |loader_factory_getter|. This doesn't
    // support reconnection to the network service after a crash, but it's OK
    // since it's used only for a single request to fetch the worker's main
    // script during startup. If the network service crashes, worker startup
    // should simply fail.
    network::mojom::URLLoaderFactoryPtr network_factory_ptr;
    loader_factory_getter->CloneNetworkFactory(
        mojo::MakeRequest(&network_factory_ptr));
    factory_bundle_for_browser_info->pending_default_factory() =
        network_factory_ptr.PassInterface();
    url_loader_factory = base::MakeRefCounted<blink::URLLoaderFactoryBundle>(
        std::move(factory_bundle_for_browser_info));
  }

  // It's safe for |appcache_handle_core| to be a raw pointer. The core is owned
  // by AppCacheNavigationHandle on the UI thread, which posts a task to delete
  // the core on the IO thread on destruction, which must happen after this
  // task.
  base::WeakPtr<AppCacheHost> appcache_host =
      appcache_handle_core ? appcache_handle_core->host()->GetWeakPtr()
                           : nullptr;

  // Create a ResourceContext getter using |service_worker_context|.
  // This context is aware of shutdown and safely returns a nullptr
  // instead of a destroyed ResourceContext in that case.
  auto resource_context_getter = base::BindRepeating(
      &ServiceWorkerContextWrapper::resource_context, service_worker_context);

  // Start loading a web worker main script.
  // TODO(nhiroki): Figure out what we should do in |wc_getter| for loading web
  // worker's main script. Returning the WebContents of the closest ancestor's
  // frame is a possible option, but it doesn't work when a shared worker
  // creates a dedicated worker after the closest ancestor's frame is gone. The
  // frame tree node ID has the same issue.
  base::RepeatingCallback<WebContents*()> wc_getter =
      base::BindRepeating([]() -> WebContents* { return nullptr; });
  std::vector<std::unique_ptr<URLLoaderThrottle>> throttles =
      GetContentClient()->browser()->CreateURLLoaderThrottlesOnIO(
          *resource_request, resource_context, wc_getter,
          nullptr /* navigation_ui_data */,
          RenderFrameHost::kNoFrameTreeNodeId);

  WorkerScriptFetcher::CreateAndStart(
      std::make_unique<WorkerScriptLoaderFactory>(
          worker_process_id,
          /*service_worker_handle=*/nullptr, service_worker_handle_core,
          std::move(appcache_host),
          base::RepeatingCallback<BrowserContext*(void)>(),
          resource_context_getter, std::move(url_loader_factory)),
      std::move(throttles), std::move(resource_request),
      base::BindOnce(WorkerScriptFetchInitiator::DidCreateScriptLoader,
                     std::move(callback),
                     std::move(subresource_loader_factories)));
}

void WorkerScriptFetchInitiator::DidCreateScriptLoader(
    CompletionCallback callback,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    bool success) {
  // This can be the UI thread or IO thread.
  DCHECK_CURRENTLY_ON(GetLoaderThreadID());

  // If a URLLoaderFactory for AppCache is supplied, use that.
  if (subresource_loader_params &&
      subresource_loader_params->pending_appcache_loader_factory) {
    subresource_loader_factories->pending_appcache_factory() =
        std::move(subresource_loader_params->pending_appcache_loader_factory);
  }

  // Prepare the controller service worker info to pass to the renderer.
  blink::mojom::ControllerServiceWorkerInfoPtr controller;
  base::WeakPtr<ServiceWorkerObjectHost> controller_service_worker_object_host;
  if (subresource_loader_params &&
      subresource_loader_params->controller_service_worker_info) {
    controller =
        std::move(subresource_loader_params->controller_service_worker_info);
    controller_service_worker_object_host =
        subresource_loader_params->controller_service_worker_object_host;
  }

  RunOrPostTask(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(
          std::move(callback), std::move(subresource_loader_factories),
          std::move(main_script_load_params), std::move(controller),
          std::move(controller_service_worker_object_host), success));
}

}  // namespace content
