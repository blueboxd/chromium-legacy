// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_
#define CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_

#include "base/macros.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_factory.mojom.h"

namespace blink {
class URLLoaderFactoryBundleInfo;
}  // namespace blink

namespace content {

class SharedWorkerFactoryImpl : public blink::mojom::SharedWorkerFactory {
 public:
  static void Create(blink::mojom::SharedWorkerFactoryRequest request);

 private:
  SharedWorkerFactoryImpl();

  // mojom::SharedWorkerFactory methods:
  void CreateSharedWorker(
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
      service_manager::mojom::InterfaceProviderPtr interface_provider) override;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerFactoryImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_SHARED_WORKER_FACTORY_IMPL_H_
