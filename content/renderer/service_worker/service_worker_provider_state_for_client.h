// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_STATE_FOR_CLIENT_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_STATE_FOR_CLIENT_H_

#include <stdint.h>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/renderer/service_worker/web_service_worker_provider_impl.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"

namespace network {
class WeakWrapperSharedURLLoaderFactory;
}

namespace content {

// Holds state for ServiceWorkerProviderContext instances for service worker
// clients.
struct ServiceWorkerProviderStateForClient {
  explicit ServiceWorkerProviderStateForClient(
      scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory);
  ~ServiceWorkerProviderStateForClient();

  // |controller| will be set by SetController() and taken by TakeController().
  blink::mojom::ServiceWorkerObjectInfoPtr controller;
  // Keeps version id of the current controller service worker object.
  int64_t controller_version_id = blink::mojom::kInvalidServiceWorkerVersionId;

  // Used to intercept requests from the controllee and dispatch them
  // as events to the controller ServiceWorker.
  network::mojom::URLLoaderFactoryPtr subresource_loader_factory;

  // Used when we create |subresource_loader_factory|.
  scoped_refptr<network::SharedURLLoaderFactory> fallback_loader_factory;

  // Used to ensure handed out loader factories are properly detached when the
  // contained subresource_loader_factory goes away.
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      weak_wrapped_subresource_loader_factory;

  // The Client#id value of the client.
  std::string client_id;

  base::UnguessableToken fetch_request_window_id;

  blink::mojom::ControllerServiceWorkerMode controller_mode =
      blink::mojom::ControllerServiceWorkerMode::kNoController;

  // Tracks feature usage for UseCounter.
  std::set<blink::mojom::WebFeature> used_features;

  // Corresponds to this client's ServiceWorkerContainer. May be null when not
  // yet created, when already destroyed, or when this client is not a Document
  // and therefore doesn't support navigator.serviceWorker.
  base::WeakPtr<WebServiceWorkerProviderImpl> web_service_worker_provider;

  // Keeps ServiceWorkerWorkerClient pointers of dedicated or shared workers
  // which are associated with the ServiceWorkerProviderContext.
  // - If this ServiceWorkerProviderContext is for a Document, then
  //   |worker_clients| contains all its dedicated workers.
  // - If this ServiceWorkerProviderContext is for a SharedWorker (technically
  //   speaking, for its shadow page), then |worker_clients| has one element:
  //   the shared worker.
  std::vector<blink::mojom::ServiceWorkerWorkerClientPtr> worker_clients;

  // For adding new ServiceWorkerWorkerClients.
  mojo::BindingSet<blink::mojom::ServiceWorkerWorkerClientRegistry>
      worker_client_registry_bindings;

  // Used in |subresource_loader_factory| to get the connection to the
  // controller service worker.
  //
  // |controller_endpoint| is a Mojo pipe to the controller service worker,
  // and is to be passed to (i.e. taken by) a subresource loader factory when
  // GetSubresourceLoaderFactory() is called for the first time when a valid
  // controller exists.
  //
  // |controller_connector| is a Mojo pipe to the
  // ControllerServiceWorkerConnector that is attached to the newly created
  // subresource loader factory and lives on a background thread. This is
  // populated when GetSubresourceLoader() creates the subresource loader
  // factory and takes |controller_endpoint|.
  blink::mojom::ControllerServiceWorkerPtrInfo controller_endpoint;
  blink::mojom::ControllerServiceWorkerConnectorPtr controller_connector;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_STATE_FOR_CLIENT_H_
