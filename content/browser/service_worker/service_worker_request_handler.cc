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
    ServiceWorkerNavigationHandle* navigation_handle,
    const NavigationRequestInfo& request_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!url.SchemeIsHTTPOrHTTPS() && !OriginCanAccessServiceWorkers(url) &&
      !SchemeMaySupportRedirectingToHTTPS(url)) {
    return nullptr;
  }

  return std::make_unique<ServiceWorkerNavigationLoaderInterceptor>(
      request_info, navigation_handle);
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

  auto provider_info = blink::mojom::ServiceWorkerProviderInfoForWindow::New();
  // Initialize the SWProviderHost.
  *out_provider_host = ServiceWorkerProviderHost::PreCreateNavigationHost(
      context->AsWeakPtr(), request_info.are_ancestors_secure,
      request_info.frame_tree_node_id, &provider_info);
  navigation_handle_core->OnCreatedProviderHost(*out_provider_host,
                                                std::move(provider_info));

  const ResourceType resource_type = request_info.is_main_frame
                                         ? ResourceType::kMainFrame
                                         : ResourceType::kSubFrame;
  return (*out_provider_host)
      ->CreateLoaderInterceptor(resource_type,
                                request_info.begin_params->skip_service_worker);
}

// static
std::unique_ptr<NavigationLoaderInterceptor>
ServiceWorkerRequestHandler::CreateForWorker(
    const network::ResourceRequest& resource_request,
    ServiceWorkerProviderHost* host) {
  DCHECK(host);
  DCHECK(resource_request.resource_type ==
             static_cast<int>(ResourceType::kWorker) ||
         resource_request.resource_type ==
             static_cast<int>(ResourceType::kSharedWorker))
      << resource_request.resource_type;

  // Create the handler even for insecure HTTP since it's used in the
  // case of redirect to HTTPS.
  if (!resource_request.url.SchemeIsHTTPOrHTTPS() &&
      !OriginCanAccessServiceWorkers(resource_request.url)) {
    return nullptr;
  }

  return host->CreateLoaderInterceptor(
      static_cast<ResourceType>(resource_request.resource_type),
      resource_request.skip_service_worker);
}

}  // namespace content
