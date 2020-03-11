// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom.h"
#include "url/origin.h"

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerVersion;

// ServiceWorkerProviderHost is the host of a service worker execution context
// in the renderer process. One ServiceWorkerProviderHost instance hosts one
// service worker execution context instance.
//
// ServiceWorkerProviderHost lives on the service worker core thread, since all
// nearly all browser process service worker machinery lives on the service
// worker core thread.
//
// TODO(https://crbug.com/931087): Rename this to ServiceWorkerHost.
class CONTENT_EXPORT ServiceWorkerProviderHost {
 public:
  ServiceWorkerProviderHost(
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          host_receiver,
      ServiceWorkerVersion* running_hosted_version,
      base::WeakPtr<ServiceWorkerContextCore> context);
  ~ServiceWorkerProviderHost();

  int provider_id() const { return provider_id_; }
  int worker_process_id() const { return worker_process_id_; }
  ServiceWorkerVersion* running_hosted_version() const {
    return running_hosted_version_;
  }

  // Completes initialization of this provider host. It is called once a
  // renderer process has been found to host the worker.
  void CompleteStartWorkerPreparation(
      int process_id,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          broker_receiver);

  void CreateQuicTransportConnector(
      mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver);
  // Used only when EagerCacheStorageSetupForServiceWorkers is disabled.
  void BindCacheStorage(
      mojo::PendingReceiver<blink::mojom::CacheStorage> receiver);

  content::ServiceWorkerContainerHost* container_host() {
    return container_host_.get();
  }

  base::WeakPtr<ServiceWorkerProviderHost> GetWeakPtr();

  void ReportNoBinderForInterface(const std::string& error);

 private:
  // Unique among all provider hosts.
  const int provider_id_;

  int worker_process_id_ = ChildProcessHost::kInvalidUniqueID;

  // The instance of service worker this provider hosts.
  // Raw pointer is safe because the version owns |this|.
  ServiceWorkerVersion* const running_hosted_version_;

  BrowserInterfaceBrokerImpl<ServiceWorkerProviderHost,
                             const ServiceWorkerVersionInfo&>
      broker_{this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  std::unique_ptr<ServiceWorkerContainerHost> container_host_;

  base::WeakPtrFactory<ServiceWorkerProviderHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
