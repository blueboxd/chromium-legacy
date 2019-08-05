// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_request_handler.h"

#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_controllee_request_handler.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/service_worker/service_worker_navigation_handle_core.h"
#include "content/browser/service_worker/service_worker_navigation_loader_interceptor.h"
#include "content/browser/service_worker/service_worker_provider_host.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/url_constants.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/gurl.h"

namespace content {

namespace {

bool SchemeMaySupportRedirectingToHTTPS(const GURL& url) {
#if defined(OS_CHROMEOS)
  return url.SchemeIs(kExternalFileScheme);
#else   // OS_CHROMEOS
  return false;
#endif  // OS_CHROMEOS
}

}  // namespace

// static
std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerRequestHandler::CreateForNavigationUI(
    const GURL& url,
    base::WeakPtr<ServiceWorkerNavigationHandle> navigation_handle,
    const NavigationRequestInfo& request_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!url.SchemeIsHTTPOrHTTPS() && !OriginCanAccessServiceWorkers(url) &&
      !SchemeMaySupportRedirectingToHTTPS(url)) {
    return nullptr;
  }

  ServiceWorkerNavigationLoaderInterceptorParams params;
  params.resource_type = request_info.is_main_frame ? ResourceType::kMainFrame
                                                    : ResourceType::kSubFrame;
  params.skip_service_worker = request_info.begin_params->skip_service_worker;
  params.is_main_frame = request_info.is_main_frame;
  params.are_ancestors_secure = request_info.are_ancestors_secure;
  params.frame_tree_node_id = request_info.frame_tree_node_id;

  return std::make_unique<ServiceWorkerNavigationLoaderInterceptor>(
      params, std::move(navigation_handle));
}

// static
std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerRequestHandler::CreateForNavigationIO(
    const GURL& url,
    ServiceWorkerNavigationHandleCore* navigation_handle_core,
    const NavigationRequestInfo& request_info,
    base::WeakPtr<ServiceWorkerProviderHost>* out_provider_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(navigation_handle_core);

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!url.SchemeIsHTTPOrHTTPS() && !OriginCanAccessServiceWorkers(url) &&
      !SchemeMaySupportRedirectingToHTTPS(url)) {
    return nullptr;
  }

  if (!navigation_handle_core->context_wrapper())
    return nullptr;
  ServiceWorkerContextCore* context =
      navigation_handle_core->context_wrapper()->context();
  if (!context)
    return nullptr;

  blink::mojom::ServiceWorkerContainerAssociatedPtrInfo client_ptr_info;
  blink::mojom::ServiceWorkerContainerHostAssociatedRequest host_request;

  auto provider_info = blink::mojom::ServiceWorkerProviderInfoForClient::New();
  provider_info->client_request = mojo::MakeRequest(&client_ptr_info);
  host_request = mojo::MakeRequest(&provider_info->host_ptr_info);

  // Initialize the SWProviderHost.
  *out_provider_host = ServiceWorkerProviderHost::PreCreateNavigationHost(
      context->AsWeakPtr(), request_info.are_ancestors_secure,
      request_info.frame_tree_node_id, std::move(host_request),
      std::move(client_ptr_info));
  navigation_handle_core->OnCreatedProviderHost(*out_provider_host,
                                                std::move(provider_info));

  const ResourceType resource_type = request_info.is_main_frame
                                         ? ResourceType::kMainFrame
                                         : ResourceType::kSubFrame;
  return std::make_unique<ServiceWorkerControlleeRequestHandler>(
      context->AsWeakPtr(), *out_provider_host, resource_type,
      request_info.begin_params->skip_service_worker);
}

// static
std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerRequestHandler::CreateForWorkerUI(
    const network::ResourceRequest& resource_request,
    int process_id,
    base::WeakPtr<ServiceWorkerNavigationHandle> navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto resource_type =
      static_cast<ResourceType>(resource_request.resource_type);
  DCHECK(resource_type == ResourceType::kWorker ||
         resource_type == ResourceType::kSharedWorker)
      << resource_request.resource_type;

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!resource_request.url.SchemeIsHTTPOrHTTPS() &&
      !OriginCanAccessServiceWorkers(resource_request.url)) {
    return nullptr;
  }

  ServiceWorkerNavigationLoaderInterceptorParams params;
  params.resource_type = resource_type;
  params.skip_service_worker = resource_request.skip_service_worker;
  params.process_id = process_id;

  return std::make_unique<ServiceWorkerNavigationLoaderInterceptor>(
      params, std::move(navigation_handle));
}

// static
std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerRequestHandler::CreateForWorkerIO(
    const network::ResourceRequest& resource_request,
    int process_id,
    ServiceWorkerNavigationHandleCore* navigation_handle_core) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!resource_request.url.SchemeIsHTTPOrHTTPS() &&
      !OriginCanAccessServiceWorkers(resource_request.url)) {
    return nullptr;
  }

  if (!navigation_handle_core->context_wrapper())
    return nullptr;
  ServiceWorkerContextCore* context =
      navigation_handle_core->context_wrapper()->context();
  if (!context)
    return nullptr;

  auto resource_type =
      static_cast<ResourceType>(resource_request.resource_type);
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
      NOTREACHED() << resource_request.resource_type;
      return nullptr;
  }

  blink::mojom::ServiceWorkerContainerAssociatedPtrInfo client_ptr_info;
  blink::mojom::ServiceWorkerContainerHostAssociatedRequest host_request;

  auto provider_info = blink::mojom::ServiceWorkerProviderInfoForClient::New();
  provider_info->client_request = mojo::MakeRequest(&client_ptr_info);
  host_request = mojo::MakeRequest(&provider_info->host_ptr_info);

  // Initialize the SWProviderHost.
  base::WeakPtr<ServiceWorkerProviderHost> host =
      ServiceWorkerProviderHost::PreCreateForWebWorker(
          context->AsWeakPtr(), process_id, provider_type,
          std::move(host_request), std::move(client_ptr_info));
  navigation_handle_core->OnCreatedProviderHost(host, std::move(provider_info));

  return std::make_unique<ServiceWorkerControlleeRequestHandler>(
      context->AsWeakPtr(), host, resource_type,
      resource_request.skip_service_worker);
}

}  // namespace content
