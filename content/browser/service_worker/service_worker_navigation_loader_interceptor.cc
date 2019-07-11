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
#include "content/browser/navigation_subresource_loader_params.h"
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

using SetupCallback =
    base::OnceCallback<void(blink::mojom::ServiceWorkerProviderInfoForWindowPtr,
                            std::unique_ptr<NavigationLoaderInterceptor,
                                            BrowserThread::DeleteOnIOThread>)>;

// Does setup on the IO thread and calls |callback| on the UI thread.
void InitOnIO(ServiceWorkerNavigationHandleCore* handle_core,
              bool are_ancestors_secure,
              int frame_tree_node_id,
              ResourceType resource_type,
              bool skip_service_worker,
              SetupCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerContextCore* context_core =
      handle_core->context_wrapper()->context();
  if (!context_core) {
    PostTaskWithTraits(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(std::move(callback), /*provider_info=*/nullptr,
                       /*interceptor=*/nullptr));
    return;
  }

  // Make the provider host.
  auto provider_info = blink::mojom::ServiceWorkerProviderInfoForWindow::New();
  base::WeakPtr<ServiceWorkerProviderHost> provider_host =
      ServiceWorkerProviderHost::PreCreateNavigationHost(
          context_core->AsWeakPtr(), are_ancestors_secure, frame_tree_node_id,
          &provider_info);
  handle_core->set_provider_host(provider_host);

  // TODO(crbug.com/926114): Respect |skip_service_worker|.

  // Make the inner interceptor.
  std::unique_ptr<ServiceWorkerControlleeRequestHandler,
                  BrowserThread::DeleteOnIOThread>
      interceptor(new ServiceWorkerControlleeRequestHandler(
          context_core->AsWeakPtr(), provider_host, resource_type));

  PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(std::move(callback), std::move(provider_info),
                     std::move(interceptor)));
}

void LoaderCallbackWrapperOnIO(
    base::WeakPtr<ServiceWorkerNavigationLoaderInterceptor> interceptor_on_ui,
    SingleRequestURLLoaderFactory::RequestHandler handler) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(
          &ServiceWorkerNavigationLoaderInterceptor::LoaderCallbackWrapper,
          interceptor_on_ui, std::move(handler)));
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

  // First make the inner interceptor if it hasn't been done yet. We'll
  // come back here after.
  if (!interceptor_on_io_) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &InitOnIO, handle_->core(), are_ancestors_secure_,
            frame_tree_node_id_, resource_type_, skip_service_worker_,
            base::BindOnce(
                &ServiceWorkerNavigationLoaderInterceptor::OnInitComplete,
                GetWeakPtr(), tentative_resource_request, browser_context,
                resource_context, std::move(loader_callback),
                std::move(fallback_callback))));
    return;
  }

  // Start the inner interceptor on the IO thread. It will call back to
  // functions on the IO thread which hop back to LoaderCallbackWrapper() or
  // FallbackCallbackWrapper() on the UI thread.
  loader_callback_ = std::move(loader_callback);
  fallback_callback_ = std::move(fallback_callback);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &NavigationLoaderInterceptor::MaybeCreateLoader,
          base::Unretained(interceptor_on_io_.get()),
          tentative_resource_request, browser_context, resource_context,
          base::BindOnce(&LoaderCallbackWrapperOnIO, GetWeakPtr()),
          base::BindOnce(&FallbackCallbackWrapperOnIO, GetWeakPtr())));
}

base::Optional<SubresourceLoaderParams>
ServiceWorkerNavigationLoaderInterceptor::MaybeCreateSubresourceLoaderParams() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(crbug.com/926114): Implement this. The params have to be received from
  // the IO thread inner interceptor before MaybeCreateLoader() is completed, so
  // this function can synchronously return them.
  return base::nullopt;
}

void ServiceWorkerNavigationLoaderInterceptor::OnInitComplete(
    const network::ResourceRequest& tentative_resource_request,
    BrowserContext* browser_context,
    ResourceContext* resource_context,
    LoaderCallback callback,
    FallbackCallback fallback_callback,
    blink::mojom::ServiceWorkerProviderInfoForWindowPtr provider_info,
    std::unique_ptr<NavigationLoaderInterceptor,
                    BrowserThread::DeleteOnIOThread> interceptor) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(!interceptor_on_io_);
  interceptor_on_io_ = std::move(interceptor);

  handle_->OnCreatedProviderHost(std::move(provider_info));
  MaybeCreateLoader(tentative_resource_request, browser_context,
                    resource_context, std::move(callback),
                    std::move(fallback_callback));
}

void ServiceWorkerNavigationLoaderInterceptor::LoaderCallbackWrapper(
    SingleRequestURLLoaderFactory::RequestHandler handler_on_io) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
