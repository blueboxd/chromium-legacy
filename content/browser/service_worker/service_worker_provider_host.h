// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "content/browser/browser_interface_broker_impl.h"
#include "content/browser/service_worker/service_worker_object_host.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/resource_type.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-forward.h"
#include "third_party/blink/public/mojom/locks/lock_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/mojom/webtransport/quic_transport_connector.mojom.h"
#include "url/origin.h"

namespace service_worker_object_host_unittest {
class ServiceWorkerObjectHostTest;
}

namespace content {

class ServiceWorkerContainerHost;
class ServiceWorkerContextCore;
class ServiceWorkerVersion;

// ServiceWorkerProviderHost is the browser-process representation of a
// renderer-process entity that can involve service workers. Currently, these
// entities are frames or workers. So basically, one ServiceWorkerProviderHost
// instance is the browser process's source of truth about one frame/worker in a
// renderer process, which the browser process uses when performing operations
// involving service workers.
//
// ServiceWorkerProviderHost lives on the IO thread, since all nearly all
// browser process service worker machinery lives on the IO thread.
//
// Example:
// * A new service worker registration is created. The browser process loops
// over all ServiceWorkerProviderHosts to find clients (frames and shared
// workers) with a URL inside the registration's scope, and has the provider
// host watch the registration in order to resolve navigator.serviceWorker.ready
// once the registration settles, if neeed.
//
// "Provider" is a somewhat tricky term. The idea is that a provider is what
// attaches to a frame/worker and "provides" it with functionality related to
// service workers. This functionality is mostly granted by creating the
// ServiceWorkerProviderHost for this frame/worker, which, again, makes the
// frame/worker alive in the browser's service worker world.
//
// A provider host has a Mojo connection to the provider in the renderer.
// Destruction of the host happens upon disconnection of the Mojo pipe.
//
// There are two general types of providers:
// 1) those for service worker clients (windows or shared workers), and
// 2) those for service workers themselves.
//
// For client providers, there is a provider (ServiceWorkerProviderContext) per
// frame or shared worker in the renderer process. The lifetime of this host
// object is tied to the lifetime of the document or the worker.
//
// For service worker providers, there is a provider
// (ServiceWorkerContextClient) per running service worker in the renderer
// process. The lifetime of this host object is tied to the lifetime of the
// running service worker.
//
// A ServiceWorkerProviderHost is created in the following situations:
//
// 1) For a client created for a navigation (for both top-level and
// non-top-level frames), the provider host for the resulting document is
// pre-created by the browser process and the provider info is sent in the
// navigation commit IPC.
//
// 2) For web workers and for service workers, the provider host is
// created by the browser process and the provider info is sent in the start
// worker IPC message.
//
// TODO(https://crbug.com/931087): Rename this to ServiceWorkerHost.
class CONTENT_EXPORT ServiceWorkerProviderHost
    : public base::SupportsWeakPtr<ServiceWorkerProviderHost>,
      public service_manager::mojom::InterfaceProvider {
 public:
  // Used to pre-create a ServiceWorkerProviderHost for a navigation. The
  // ServiceWorkerProviderContext will later be created in the renderer, should
  // the navigation succeed. |are_ancestors_secure| should be true for main
  // frames. Otherwise it is true iff all ancestor frames of this frame have a
  // secure origin. |frame_tree_node_id| is FrameTreeNode
  // id. |web_contents_getter| indicates the tab where the navigation is
  // occurring.
  //
  // The returned host stays alive as long as the corresponding host ptr for
  // |host_request| stays alive.
  static base::WeakPtr<ServiceWorkerProviderHost> PreCreateNavigationHost(
      base::WeakPtr<ServiceWorkerContextCore> context,
      bool are_ancestors_secure,
      int frame_tree_node_id,
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          host_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          container_remote);

  // Used for starting a service worker. Returns a provider host for the service
  // worker and partially fills |out_provider_info|.  The host stays alive as
  // long as this info stays alive (namely, as long as
  // |out_provider_info->host_remote| stays alive).
  // CompleteStartWorkerPreparation() must be called later to get a full info to
  // send to the renderer.
  static base::WeakPtr<ServiceWorkerProviderHost> CreateForServiceWorker(
      base::WeakPtr<ServiceWorkerContextCore> context,
      scoped_refptr<ServiceWorkerVersion> version,
      blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr*
          out_provider_info);

  // Used for starting a web worker (dedicated worker or shared worker). Returns
  // a provider host for the worker. The host stays alive as long as the
  // corresponding host for |host_receiver| stays alive.
  static base::WeakPtr<ServiceWorkerProviderHost> CreateForWebWorker(
      base::WeakPtr<ServiceWorkerContextCore> context,
      int process_id,
      blink::mojom::ServiceWorkerProviderType provider_type,
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          host_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          container_remote);

  ~ServiceWorkerProviderHost() override;

  // May return nullptr.
  RenderProcessHost* GetProcessHost() {
    DCHECK(IsProviderForServiceWorker());
    return RenderProcessHost::FromID(worker_process_id_);
  }

  int provider_id() const { return provider_id_; }

  // For service worker execution contexts. The version of the service worker.
  // This is nullptr when the worker is still starting up (until
  // CompleteStartWorkerPreparation() is called).
  ServiceWorkerVersion* running_hosted_version() const;

  // TODO(https://crbug.com/931087): Remove these functions in favor of the
  // equivalent functions on ServiceWorkerContainerHost.
  blink::mojom::ServiceWorkerProviderType provider_type() const;
  bool IsProviderForServiceWorker() const;

  // May return nullptr if the context has shut down.
  base::WeakPtr<ServiceWorkerContextCore> context() { return context_; }

  // For service worker execution contexts. Completes initialization of this
  // provider host. It is called once a renderer process has been found to host
  // the worker.
  void CompleteStartWorkerPreparation(
      int process_id,
      mojo::PendingReceiver<service_manager::mojom::InterfaceProvider>
          interface_provider_receiver,
      mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
          broker_receiver);

  // For service worker execution contexts.
  void CreateQuicTransportConnector(
      mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver);

  content::ServiceWorkerContainerHost* container_host() {
    return container_host_.get();
  }

 private:
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;
  FRIEND_TEST_ALL_PREFIXES(BackgroundSyncManagerTest,
                           RegisterWithoutLiveSWRegistration);

  static void RegisterToContextCore(
      base::WeakPtr<ServiceWorkerContextCore> context,
      std::unique_ptr<ServiceWorkerProviderHost> host);

  ServiceWorkerProviderHost(
      blink::mojom::ServiceWorkerProviderType type,
      bool is_parent_frame_secure,
      int frame_tree_node_id,
      mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
          host_receiver,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          container_remote,
      scoped_refptr<ServiceWorkerVersion> running_hosted_version,
      base::WeakPtr<ServiceWorkerContextCore> context);

  // service_manager::mojom::InterfaceProvider:
  // For service worker execution contexts.
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // For service worker execution contexts. Sets the process ID of the service
  // worker execution context.
  void SetWorkerProcessId(int process_id);

  // Unique among all provider hosts.
  const int provider_id_;

  int worker_process_id_ = ChildProcessHost::kInvalidUniqueID;

  // For service worker execution contexts. The ServiceWorkerVersion of the
  // service worker this is a provider for.
  const scoped_refptr<ServiceWorkerVersion> running_hosted_version_;

  // |context_| owns |this| but if the context is destroyed and a new one is
  // created, the provider host becomes owned by the new context, while this
  // |context_| is reset to null.
  // TODO(https://crbug.com/877356): Don't support copying context, so this can
  // just be a raw ptr that is never null.
  base::WeakPtr<ServiceWorkerContextCore> context_;

  // For service worker execution contexts.
  mojo::Binding<service_manager::mojom::InterfaceProvider>
      interface_provider_binding_;
  BrowserInterfaceBrokerImpl<ServiceWorkerProviderHost,
                             const ServiceWorkerVersionInfo&>
      broker_{this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  // TODO(https://crbug.com/931087): Make an execution context host (e.g.,
  // RenderFrameHostImpl) own this container host.
  std::unique_ptr<content::ServiceWorkerContainerHost> container_host_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
