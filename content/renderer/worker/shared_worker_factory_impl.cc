// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/shared_worker_factory_impl.h"

#include "base/memory/ptr_util.h"
#include "content/renderer/worker/embedded_shared_worker_stub.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"

namespace content {

// static
void SharedWorkerFactoryImpl::Create(
    blink::mojom::SharedWorkerFactoryRequest request) {
  mojo::MakeStrongBinding<blink::mojom::SharedWorkerFactory>(
      base::WrapUnique(new SharedWorkerFactoryImpl()), std::move(request));
}

SharedWorkerFactoryImpl::SharedWorkerFactoryImpl() {}

void SharedWorkerFactoryImpl::CreateSharedWorker(
    blink::mojom::SharedWorkerInfoPtr info,
    bool pause_on_start,
    const base::UnguessableToken& devtools_worker_token,
    blink::mojom::RendererPreferencesPtr renderer_preferences,
    blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request,
    blink::mojom::WorkerContentSettingsProxyPtr content_settings,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr
        service_worker_provider_info,
    const base::Optional<base::UnguessableToken>& appcache_host_id,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    blink::mojom::SharedWorkerHostPtr host,
    blink::mojom::SharedWorkerRequest request,
    service_manager::mojom::InterfaceProviderPtr interface_provider) {
  // Bound to the lifetime of the underlying blink::WebSharedWorker instance.
  new EmbeddedSharedWorkerStub(
      std::move(info), pause_on_start, devtools_worker_token,
      *renderer_preferences, std::move(preference_watcher_request),
      std::move(content_settings), std::move(service_worker_provider_info),
      appcache_host_id.value_or(base::UnguessableToken()),
      std::move(main_script_load_params),
      std::move(subresource_loader_factories), std::move(controller_info),
      std::move(host), std::move(request), std::move(interface_provider));
}

}  // namespace content
