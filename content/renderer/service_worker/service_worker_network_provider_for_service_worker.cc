// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/service_worker/service_worker_network_provider_for_service_worker.h"

#include <utility>

#include "content/public/common/resource_type.h"
#include "content/public/renderer/url_loader_throttle_provider.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/loader/web_url_loader_impl.h"
#include "content/renderer/loader/web_url_request_util.h"
#include "content/renderer/render_thread_impl.h"
#include "ipc/ipc_message.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace content {

ServiceWorkerNetworkProviderForServiceWorker::
    ServiceWorkerNetworkProviderForServiceWorker(
        network::mojom::URLLoaderFactoryPtrInfo script_loader_factory_info)
    : script_loader_factory_(std::move(script_loader_factory_info)) {}

ServiceWorkerNetworkProviderForServiceWorker::
    ~ServiceWorkerNetworkProviderForServiceWorker() = default;

void ServiceWorkerNetworkProviderForServiceWorker::WillSendRequest(
    blink::WebURLRequest& request) {
  auto extra_data = std::make_unique<RequestExtraData>();
  extra_data->set_originated_from_service_worker(true);

  // The RenderThreadImpl or its URLLoaderThrottleProvider member may not be
  // valid in some tests.
  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  if (render_thread && render_thread->url_loader_throttle_provider()) {
    extra_data->set_url_loader_throttles(
        render_thread->url_loader_throttle_provider()->CreateThrottles(
            MSG_ROUTING_NONE, request, WebURLRequestToResourceType(request)));
  }

  request.SetExtraData(std::move(extra_data));
}

std::unique_ptr<blink::WebURLLoader>
ServiceWorkerNetworkProviderForServiceWorker::CreateURLLoader(
    const blink::WebURLRequest& request,
    std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
        task_runner_handle) {
  if (request.GetRequestContext() !=
      blink::mojom::RequestContextType::SERVICE_WORKER) {
    // This provider is only used for requests from the shadow page, which is
    // created to load the service worker's main script. But shadow pages
    // sometimes request strange things like CSS resources because consumers
    // think it's a real frame. Just return nullptr to use the default loader
    // instead of the script loader.
    return nullptr;
  }

  RenderThreadImpl* render_thread = RenderThreadImpl::current();
  // RenderThreadImpl may be null in some tests.
  if (render_thread && script_loader_factory()) {
    // TODO(crbug.com/796425): Temporarily wrap the raw
    // mojom::URLLoaderFactory pointer into SharedURLLoaderFactory.
    return std::make_unique<WebURLLoaderImpl>(
        render_thread->resource_dispatcher(), std::move(task_runner_handle),
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            script_loader_factory()));
  }
  return nullptr;
}

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerNetworkProviderForServiceWorker::GetControllerServiceWorkerMode() {
  return blink::mojom::ControllerServiceWorkerMode::kNoController;
}

int64_t
ServiceWorkerNetworkProviderForServiceWorker::ControllerServiceWorkerID() {
  return blink::mojom::kInvalidServiceWorkerVersionId;
}

void ServiceWorkerNetworkProviderForServiceWorker::DispatchNetworkQuiet() {}

}  // namespace content
