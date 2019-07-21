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
#include "content/browser/loader/navigation_url_loader_impl.h"
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
    blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
    SingleRequestURLLoaderFactory::RequestHandler handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::Optional<SubresourceLoaderParams> subresource_loader_params;
  if (handle_core->interceptor()) {
    subresource_loader_params =
        handle_core->interceptor()->MaybeCreateSubresourceLoaderParams();
  }

  PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::LoaderCallbackWrapper,
          interceptor_on_ui, std::move(provider_info),
          std::move(subresource_loader_params), std::move(handler)));
}

void FallbackCallbackWrapperOnIO(
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    bool reset_subresource_loader_params) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::FallbackCallbackWrapper,
          interceptor_on_ui, reset_subresource_loader_params));
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
    bool are_ancestors_secure,
    int frame_tree_node_id,
    ResourceType resource_type,
    bool skip_service_worker,
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerContextCore* context_core =
      handle_core->context_wrapper()->context();
  ResourceContext* resource_context =
      handle_core->context_wrapper()->resource_context();
  if (!context_core || !resource_context) {
    LoaderCallbackWrapperOnIO(handle_core, std::move(interceptor_on_ui),
                              /*provider_info=*/nullptr,
                              /*handler=*/{});
    return;
  }

  blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info;
  base::WeakPtr<ServiceWorkerProviderHost> provider_host;

  if (!handle_core->provider_host()) {
    // This is the initial request before redirects, so make the provider host.
    // Its lifetime is tied to |provider_info| which will be passed to the
    // ServiceWorkerNavigationHandle on the UI thread, and finally passed to the
    // renderer when the navigation commits.
    provider_info = blink::mojom::ServiceWorkerProviderInfoForClient::New();
    provider_host = ServiceWorkerProviderHost::PreCreateNavigationHost(
        context_core->AsWeakPtr(), are_ancestors_secure, frame_tree_node_id,
        &provider_info);
    handle_core->set_provider_host(provider_host);

    // Also make the inner interceptor.
    DCHECK(!handle_core->interceptor());
    handle_core->set_interceptor(
        std::make_unique<ServiceWorkerControlleeRequestHandler>(
            context_core->AsWeakPtr(), provider_host, resource_type,
            skip_service_worker));
  }

  // Start the inner interceptor. We continue in LoaderCallbackWrapperOnIO().
  //
  // It's safe to bind the raw |handle_core| to the callback because it owns the
  // interceptor, which invokes the callback.
  handle_core->interceptor()->MaybeCreateLoader(
      tentative_resource_request, browser_context, resource_context,
      base::BindOnce(&LoaderCallbackWrapperOnIO, handle_core, interceptor_on_ui,
                     std::move(provider_info)),
      base::BindOnce(&FallbackCallbackWrapperOnIO, interceptor_on_ui));
}
///////////////////////////////////////////////////////////////////////////////

}  // namespace

ServiceWorkerNavigationLoaderInterceptor::
    ServiceWorkerNavigationLoaderInterceptor(
        const NavigationRequestInfo& request_info,
        ServiceWorkerNavigationHandle* handle)
    : handle_(handle),
      are_ancestors_secure_(request_info.are_ancestors_secure),
      frame_tree_node_id_(request_info.frame_tree_node_id),
      resource_type_(request_info.is_main_frame ? ResourceType::kMainFrame
                                                : ResourceType::kSubFrame),
      skip_service_worker_(request_info.begin_params->skip_service_worker) {
  DCHECK(NavigationURLLoaderImpl::IsNavigationLoaderOnUIEnabled());
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

ServiceWorkerNavigationLoaderInterceptor::
    ~ServiceWorkerNavigationLoaderInterceptor() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void ServiceWorkerNavigationLoaderInterceptor::MaybeCreateLoader(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    ResourceContext* resource_context,
    LoaderCallback loader_callback,
    FallbackCallback fallback_callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!resource_context);

  // Start the inner interceptor on the IO thread. It will call back to
  // LoaderCallbackWrapper() on the UI thread.
  loader_callback_ = std::move(loader_callback);
  fallback_callback_ = std::move(fallback_callback);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&MaybeCreateLoaderOnIO, GetWeakPtr(), handle_->core(),
                     are_ancestors_secure_, frame_tree_node_id_, resource_type_,
                     skip_service_worker_, tentative_resource_request,
                     browser_context));
}

base::Optional<SubresourceLoaderParams>
ServiceWorkerNavigationLoaderInterceptor::MaybeCreateSubresourceLoaderParams() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return std::move(subresource_loader_params_);
}

void ServiceWorkerNavigationLoaderInterceptor::LoaderCallbackWrapper(
    blink::mojom::ServiceWorkerProviderInfoForClientPtr provider_info,
    base::Optional<SubresourceLoaderParams> subresource_loader_params,
    SingleRequestURLLoaderFactory::RequestHandler handler_on_io) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // |provider_info| is non-null if this is the first request before redirects,
  // which makes the provider host for this navigation.
  if (provider_info)
    handle_->OnCreatedProviderHost(std::move(provider_info));

  subresource_loader_params_ = std::move(subresource_loader_params);

  if (!handler_on_io) {
    std::move(loader_callback_).Run({});
    return;
  }

  // The inner IO thread interceptor wants to handle the request. However,
  // |handler_on_io| expects to run on the IO thread. Give our own wrapper to
  // the loader callback.
  std::move(loader_callback_)
      .Run(base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::RequestHandlerWrapper,
          GetWeakPtr(), std::move(handler_on_io)));
}

void ServiceWorkerNavigationLoaderInterceptor::FallbackCallbackWrapper(
    bool reset_subresource_loader_params) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::move(fallback_callback_).Run(reset_subresource_loader_params);
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
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(InvokeRequestHandlerOnIO, std::move(handler_on_io),
                     resource_request, std::move(request),
                     client.PassInterface()));
}

}  // namespace content
