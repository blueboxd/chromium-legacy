// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_navigation_loader_interceptor.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/public/browser/browser_task_traits.h"

namespace content {

namespace {

///////////////////////////////////////////////////////////////////////////////
// IO thread helpers

void LoaderCallbackWrapperOnIO(
    ServiceWorkerNavigationHandleCore* handle_core,
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    NavigationLoaderInterceptor::LoaderCallback loader_callback,
    SingleRequestURLLoaderFactory::RequestHandler handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::Optional<SubresourceLoaderParams> subresource_loader_params;
  if (handle_core->interceptor()) {
    subresource_loader_params =
        handle_core->interceptor()->MaybeCreateSubresourceLoaderParams();
  }

  base::PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::LoaderCallbackWrapper,
          interceptor_on_ui, std::move(subresource_loader_params),
          std::move(loader_callback), std::move(handler)));
}

void FallbackCallbackWrapperOnIO(
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback,
    bool reset_subresource_loader_params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PostTask(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::FallbackCallbackWrapper,
          interceptor_on_ui, std::move(fallback_callback),
          reset_subresource_loader_params));
}

void InvokeRequestHandlerOnIO(
    SingleRequestURLLoaderFactory::RequestHandler handler,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtrInfo client_info) {
  network::mojom::URLLoaderClientPtr client(std::move(client_info));
  std::move(handler).Run(resource_request, std::move(request),
                         std::move(client));
}

// Does setup on the IO thread and calls back to
// |interceptor_on_ui->LoaderCallbackWrapper()| on the UI thread.
void MaybeCreateLoaderOnIO(
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    ServiceWorkerNavigationHandleCore* handle_core,
    const ServiceWorkerNavigationLoaderInterceptorParams& params,
    blink::mojom::ServiceWorkerContainerHostAssociatedRequest host_request,
    blink::mojom::ServiceWorkerContainerAssociatedPtrInfo client_ptr_info,
    const network::ResourceRequest& tentative_resource_request,
    NavigationLoaderInterceptor::LoaderCallback loader_callback,
    NavigationLoaderInterceptor::FallbackCallback fallback_callback,
    bool initialize_provider_only) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerContextCore* context_core =
      handle_core->context_wrapper()->context();
  ResourceContext* resource_context =
      handle_core->context_wrapper()->resource_context();
  if (!context_core || !resource_context) {
    LoaderCallbackWrapperOnIO(handle_core, std::move(interceptor_on_ui),
                              std::move(loader_callback),
                              /*handler=*/{});
    return;
  }

  if (!handle_core->provider_host()) {
    // This is the initial request before redirects, so make the provider host.
    // Its lifetime is tied to the |provider_info| in the
    // ServiceWorkerNavigationHandle on the UI thread and which will be passed
    // to the renderer when the navigation commits.
    DCHECK(host_request);
    DCHECK(client_ptr_info);
    base::WeakPtr<ServiceWorkerProviderHost> provider_host;

    if (params.resource_type == ResourceType::kMainFrame ||
        params.resource_type == ResourceType::kSubFrame) {
      provider_host = ServiceWorkerProviderHost::PreCreateNavigationHost(
          context_core->AsWeakPtr(), params.are_ancestors_secure,
          params.frame_tree_node_id, std::move(host_request),
          std::move(client_ptr_info));
    } else {
      DCHECK(params.resource_type == ResourceType::kWorker ||
             params.resource_type == ResourceType::kSharedWorker);
      auto provider_type =
          params.resource_type == ResourceType::kWorker
              ? blink::mojom::ServiceWorkerProviderType::kForDedicatedWorker
              : blink::mojom::ServiceWorkerProviderType::kForSharedWorker;
      provider_host = ServiceWorkerProviderHost::PreCreateForWebWorker(
          context_core->AsWeakPtr(), params.process_id, provider_type,
          std::move(host_request), std::move(client_ptr_info));
    }
    DCHECK(provider_host);
    handle_core->set_provider_host(provider_host);

    // Also make the inner interceptor.
    DCHECK(!handle_core->interceptor());
    handle_core->set_interceptor(
        std::make_unique<ServiceWorkerControlleeRequestHandler>(
            context_core->AsWeakPtr(), provider_host, params.resource_type,
            params.skip_service_worker));
  }

  // If |initialize_provider_only| is true, we have already determined there is
  // no registered service worker on the UI thread, so just initialize the
  // provider for this request.
  if (initialize_provider_only) {
    handle_core->interceptor()->InitializeProvider(tentative_resource_request);
    LoaderCallbackWrapperOnIO(handle_core, interceptor_on_ui,
                              std::move(loader_callback),
                              /*handler=*/{});
    return;
  }

  // Start the inner interceptor. We continue in LoaderCallbackWrapperOnIO().
  //
  // It's safe to bind the raw |handle_core| to the callback because it owns the
  // interceptor, which invokes the callback.
  handle_core->interceptor()->MaybeCreateLoader(
      tentative_resource_request, resource_context,
      base::BindOnce(&LoaderCallbackWrapperOnIO, handle_core, interceptor_on_ui,
                     std::move(loader_callback)),
      base::BindOnce(&FallbackCallbackWrapperOnIO, interceptor_on_ui,
                     std::move(fallback_callback)));
}
///////////////////////////////////////////////////////////////////////////////

}  // namespace

ServiceWorkerNavigationLoaderInterceptor::
    ServiceWorkerNavigationLoaderInterceptor(
        const ServiceWorkerNavigationLoaderInterceptorParams& params,
        base::WeakPtr<ServiceWorkerNavigationHandle> handle)
    : handle_(std::move(handle)), params_(params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);
}

ServiceWorkerNavigationLoaderInterceptor::
    ~ServiceWorkerNavigationLoaderInterceptor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerNavigationLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    LoaderCallback loader_callback,
    FallbackCallback fallback_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(handle_);

  blink::mojom::ServiceWorkerContainerHostAssociatedRequest host_request;
  blink::mojom::ServiceWorkerContainerAssociatedPtrInfo client_ptr_info;

  // If this is the first request before redirects, a provider info has not yet
  // been created.
  if (!handle_->has_provider_info()) {
    auto provider_info =
        blink::mojom::ServiceWorkerProviderInfoForClient::New();
    host_request = mojo::MakeRequest(&provider_info->host_ptr_info);
    provider_info->client_request = mojo::MakeRequest(&client_ptr_info);
    handle_->OnCreatedProviderHost(std::move(provider_info));
  }

  bool initialize_provider_only = false;
  LoaderCallback original_callback;
  if (!handle_->context_wrapper()->HasRegistrationForOrigin(
          tentative_resource_request.url.GetOrigin())) {
    // We have no registrations, so it's safe to continue the request now
    // without blocking on the IO thread. Give a dummy callback to the
    // IO thread interceptor, and we'll run the original callback immediately
    // after starting it.
    original_callback = std::move(loader_callback);
    loader_callback =
        base::BindOnce([](SingleRequestURLLoaderFactory::RequestHandler) {});
    initialize_provider_only = true;
  }

  // Start the inner interceptor on the IO thread. It will call back to
  // LoaderCallbackWrapper() on the UI thread.
  base::PostTask(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&MaybeCreateLoaderOnIO, GetWeakPtr(), handle_->core(),
                     params_, std::move(host_request),
                     std::move(client_ptr_info), tentative_resource_request,
                     std::move(loader_callback), std::move(fallback_callback),
                     initialize_provider_only));

  if (original_callback)
    std::move(original_callback).Run({});
}

base::Optional<SubresourceLoaderParams>
ServiceWorkerNavigationLoaderInterceptor::MaybeCreateSubresourceLoaderParams() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::move(subresource_loader_params_);
}

void ServiceWorkerNavigationLoaderInterceptor::LoaderCallbackWrapper(
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    LoaderCallback loader_callback,
    SingleRequestURLLoaderFactory::RequestHandler handler_on_io) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // For worker main script requests, |handle_| can be destroyed during
  // interception. The initiator of this interceptor (i.e., WorkerScriptLoader)
  // will handle the case.
  // For navigation requests, this case should not happen because it's
  // guaranteed that this interceptor is destroyed before |handle_|.
  if (!handle_) {
    std::move(loader_callback).Run({});
    return;
  }

  subresource_loader_params_ = std::move(subresource_loader_params);

  if (!handler_on_io) {
    std::move(loader_callback).Run({});
    return;
  }

  // The inner IO thread interceptor wants to handle the request. However,
  // |handler_on_io| expects to run on the IO thread. Give our own wrapper to
  // the loader callback.
  std::move(loader_callback)
      .Run(base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::RequestHandlerWrapper,
          GetWeakPtr(), std::move(handler_on_io)));
}

void ServiceWorkerNavigationLoaderInterceptor::FallbackCallbackWrapper(
    FallbackCallback fallback_callback,
    bool reset_subresource_loader_params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(fallback_callback).Run(reset_subresource_loader_params);
}

base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor>
ServiceWorkerNavigationLoaderInterceptor::GetWeakPtr() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return weak_factory_.GetWeakPtr();
}

void ServiceWorkerNavigationLoaderInterceptor::RequestHandlerWrapper(
    SingleRequestURLLoaderFactory::RequestHandler handler_on_io,
    const network::ResourceRequest& resource_request,
    network::mojom::URLLoaderRequest request,
    network::mojom::URLLoaderClientPtr client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::PostTask(FROM_HERE, {BrowserThread::IO},
                 base::BindOnce(InvokeRequestHandlerOnIO,
                                std::move(handler_on_io), resource_request,
                                std::move(request), client.PassInterface()));
}

}  // namespace content
