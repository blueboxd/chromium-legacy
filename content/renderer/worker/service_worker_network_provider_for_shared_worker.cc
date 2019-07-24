// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/service_worker_network_provider_for_shared_worker.h"

#include <utility>

#include "base/feature_list.h"
#include "content/public/common/origin_util.h"
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "content/renderer/loader/request_extra_data.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"

namespace content {

std::unique_ptr<ServiceWorkerNetworkProviderForSharedWorker>
ServiceWorkerNetworkProviderForSharedWorker::Create(
    blink::mojom::ServiceWorkerProviderInfoForClientPtr info,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory,
    bool is_secure_context,
    std::unique_ptr<NavigationResponseOverrideParameters> response_override) {
  auto provider =
      base::WrapUnique(new ServiceWorkerNetworkProviderForSharedWorker(
          is_secure_context, std::move(response_override)));
  if (info) {
    provider->context_ = base::MakeRefCounted<ServiceWorkerProviderContext>(
        blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
        std::move(info->client_request), std::move(info->host_ptr_info),
        std::move(controller_info), std::move(fallback_loader_factory));
  }
  return provider;
}

ServiceWorkerNetworkProviderForSharedWorker::
    ~ServiceWorkerNetworkProviderForSharedWorker() {
  if (context())
    context()->OnNetworkProviderDestroyed();
}

void ServiceWorkerNetworkProviderForSharedWorker::WillSendRequest(
    blink::WebURLRequest& request) {
  auto extra_data = std::make_unique<RequestExtraData>();
  extra_data->set_initiated_in_secure_context(is_secure_context_);
  if (response_override_) {
    DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
    extra_data->set_navigation_response_override(std::move(response_override_));
  }
  request.SetExtraData(std::move(extra_data));
}

std::unique_ptr<blink::WebURLLoader>
ServiceWorkerNetworkProviderForSharedWorker::CreateURLLoader(
    const blink::WebURLRequest& request,
    std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
        task_runner_handle) {
  if (request.GetRequestContext() !=
      blink::mojom::RequestContextType::SHARED_WORKER) {
    // This provider is only used for requests from the shadow page, which is
    // created to load the shared worker's main script. But shadow pages
    // sometimes request strange things like CSS resources because consumers
    // think it's a real frame. Just return nullptr to use the default loader
    // instead of the script loader.
    return nullptr;
  }

  // Otherwise go to default resource loading.
  return nullptr;
}

blink::mojom::ControllerServiceWorkerMode
ServiceWorkerNetworkProviderForSharedWorker::GetControllerServiceWorkerMode() {
  if (!context())
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  return context()->GetControllerServiceWorkerMode();
}

int64_t
ServiceWorkerNetworkProviderForSharedWorker::ControllerServiceWorkerID() {
  if (!context())
    return blink::mojom::kInvalidServiceWorkerVersionId;
  return context()->GetControllerVersionId();
}

void ServiceWorkerNetworkProviderForSharedWorker::DispatchNetworkQuiet() {}

ServiceWorkerNetworkProviderForSharedWorker::
    ServiceWorkerNetworkProviderForSharedWorker(
        bool is_secure_context,
        std::unique_ptr<NavigationResponseOverrideParameters> response_override)
    : is_secure_context_(is_secure_context),
      response_override_(std::move(response_override)) {}

}  // namespace content
