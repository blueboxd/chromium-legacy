// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCH_INITIATOR_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCH_INITIATOR_H_

#include <memory>
#include <set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/resource_type.h"
#include "content/public/common/url_loader_throttle.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

namespace blink {
class URLLoaderFactoryBundleInfo;
}  // namespace blink

namespace network {
class SharedURLLoaderFactory;
class SharedURLLoaderFactoryInfo;
}  // namespace network

namespace content {

class AppCacheNavigationHandleCore;
class BrowserContext;
class ResourceContext;
class ServiceWorkerContextWrapper;
class ServiceWorkerNavigationHandle;
class ServiceWorkerNavigationHandleCore;
class ServiceWorkerObjectHost;
class StoragePartitionImpl;
class URLLoaderFactoryGetter;
struct SubresourceLoaderParams;

// PlzWorker:
// WorkerScriptFetchInitiator is the entry point of browser-side script fetch
// for WorkerScriptFetcher.
// TODO(falken): These are all static functions, it should just be a namespace
// or merged elsewhere.
class WorkerScriptFetchInitiator {
 public:
  using CompletionCallback = base::OnceCallback<void(
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>,
      blink::mojom::WorkerMainScriptLoadParamsPtr,
      blink::mojom::ControllerServiceWorkerInfoPtr,
      base::WeakPtr<ServiceWorkerObjectHost>,
      bool)>;

  // Creates a worker script fetcher and starts it. Must be called on the UI
  // thread. |callback| will be called with the result on the UI thread.
  static void Start(
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
      scoped_refptr<network::SharedURLLoaderFactory>
          url_loader_factory_override,
      StoragePartitionImpl* storage_partition,
      const std::string& storage_domain,
      CompletionCallback callback);

  // Returns the BrowserThread::ID that the WorkerScriptLoaderFactory will be
  // running on.
  // TODO(crbug.com/824840): Remove this when non-NavigationLoaderOnUI code is
  // removed.
  static BrowserThread::ID GetLoaderThreadID();

 private:
  // Creates a loader factory bundle. Must be called on the UI thread.
  static std::unique_ptr<blink::URLLoaderFactoryBundleInfo> CreateFactoryBundle(
      int worker_process_id,
      StoragePartitionImpl* storage_partition,
      const std::string& storage_domain,
      bool file_support,
      bool filesystem_url_support);

  // Adds additional request headers to |resource_request|. Must be called on
  // the UI thread.
  static void AddAdditionalRequestHeaders(
      network::ResourceRequest* resource_request,
      BrowserContext* browser_context);

  static void CreateScriptLoaderOnIO(
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
      CompletionCallback callback);

  static void CreateScriptLoaderOnUI(
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
      scoped_refptr<network::SharedURLLoaderFactory>
          url_loader_factory_override,
      CompletionCallback callback);

  static void DidCreateScriptLoader(
      CompletionCallback callback,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      base::Optional<SubresourceLoaderParams> subresource_loader_params,
      bool success);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_SCRIPT_FETCH_INITIATOR_H_
