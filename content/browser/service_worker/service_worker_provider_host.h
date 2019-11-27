// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
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
#include "mojo/public/cpp/bindings/receiver_set.h"
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
    : public ServiceWorkerRegistration::Listener,
      public base::SupportsWeakPtr<ServiceWorkerProviderHost>,
      public blink::mojom::ServiceWorkerContainerHost,
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

  // For service worker clients. Sets |url_|, |site_for_cookies_| and
  // |top_frame_origin_| and updates the client uuid if it's a cross-origin
  // transition.
  void UpdateUrls(const GURL& url,
                  const GURL& site_for_cookies,
                  const base::Optional<url::Origin>& top_frame_origin);

  // TODO(https://crbug.com/931087): Remove these functions in favor of the
  // equivalent functions on ServiceWorkerContainerHost.
  blink::mojom::ServiceWorkerProviderType provider_type() const;
  bool IsProviderForServiceWorker() const;
  bool IsProviderForClient() const;

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

  // For service worker clients. The host keeps track of all the prospective
  // longest-matching registrations, in order to resolve .ready or respond to
  // claim() attempts.
  //
  // This is subtle: it doesn't keep all registrations (e.g., from storage) in
  // memory, but just the ones that are possibly the longest-matching one. The
  // best match from storage is added at load time. That match can't uninstall
  // while this host is a controllee, so all the other stored registrations can
  // be ignored. Only a newly installed registration can claim it, and new
  // installing registrations are added as matches.
  void AddMatchingRegistration(ServiceWorkerRegistration* registration);
  void RemoveMatchingRegistration(ServiceWorkerRegistration* registration);

  // An optimized implementation of [[Match Service Worker Registration]]
  // for current document.
  ServiceWorkerRegistration* MatchRegistration() const;

  // For service worker clients. Called when |version| is the active worker upon
  // the main resource request for this client. Remembers |version| as needing
  // a Soft Update. To avoid affecting page load performance, the update occurs
  // when we get a HintToUpdateServiceWorker message from the renderer, or when
  // |this| is destroyed before receiving that message.
  //
  // Corresponds to the Handle Fetch algorithm:
  // "If request is a non-subresource request...invoke Soft Update algorithm
  // with registration."
  // https://w3c.github.io/ServiceWorker/#on-fetch-request-algorithm
  //
  // This can be called multiple times due to redirects during a main resource
  // load. All service workers are updated.
  void AddServiceWorkerToUpdate(scoped_refptr<ServiceWorkerVersion> version);

  // For service worker execution contexts.
  void CreateQuicTransportConnector(
      mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver);

  content::ServiceWorkerContainerHost* container_host() {
    return container_host_.get();
  }

 private:
  friend class ServiceWorkerProviderHostTest;
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

  // ServiceWorkerRegistration::Listener overrides.
  // TODO(https://crbug.com/931087): Move these overrides to
  // ServiceWorkerContainerHost.
  void OnVersionAttributesChanged(
      ServiceWorkerRegistration* registration,
      blink::mojom::ChangedServiceWorkerObjectsMaskPtr changed_mask,
      const ServiceWorkerRegistrationInfo& info) override;
  void OnRegistrationFailed(ServiceWorkerRegistration* registration) override;
  void OnRegistrationFinishedUninstalling(
      ServiceWorkerRegistration* registration) override;
  void OnSkippedWaiting(ServiceWorkerRegistration* registration) override;

  // Syncs matching registrations with live registrations.
  void SyncMatchingRegistrations();

#if DCHECK_IS_ON()
  bool IsMatchingRegistration(ServiceWorkerRegistration* registration) const;
#endif  // DCHECK_IS_ON()

  // Discards all references to matching registrations.
  void RemoveAllMatchingRegistrations();

  void ReturnRegistrationForReadyIfNeeded();

  // Implements blink::mojom::ServiceWorkerContainerHost.
  void Register(const GURL& script_url,
                blink::mojom::ServiceWorkerRegistrationOptionsPtr options,
                blink::mojom::FetchClientSettingsObjectPtr
                    outside_fetch_client_settings_object,
                RegisterCallback callback) override;
  void GetRegistration(const GURL& client_url,
                       GetRegistrationCallback callback) override;
  void GetRegistrations(GetRegistrationsCallback callback) override;
  void GetRegistrationForReady(
      GetRegistrationForReadyCallback callback) override;
  void EnsureControllerServiceWorker(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::mojom::ControllerServiceWorkerPurpose purpose) override;
  void CloneContainerHost(
      mojo::PendingReceiver<blink::mojom::ServiceWorkerContainerHost> receiver)
      override;
  void HintToUpdateServiceWorker() override;
  void OnExecutionReady() override;

  // Callback for ServiceWorkerContextCore::RegisterServiceWorker().
  void RegistrationComplete(const GURL& script_url,
                            const GURL& scope,
                            RegisterCallback callback,
                            int64_t trace_id,
                            mojo::ReportBadMessageCallback bad_message_callback,
                            blink::ServiceWorkerStatusCode status,
                            const std::string& status_message,
                            int64_t registration_id);
  // Callback for ServiceWorkerStorage::FindRegistrationForDocument().
  void GetRegistrationComplete(
      GetRegistrationCallback callback,
      int64_t trace_id,
      blink::ServiceWorkerStatusCode status,
      scoped_refptr<ServiceWorkerRegistration> registration);
  // Callback for ServiceWorkerStorage::GetRegistrationsForOrigin().
  void GetRegistrationsComplete(
      GetRegistrationsCallback callback,
      int64_t trace_id,
      blink::ServiceWorkerStatusCode status,
      const std::vector<scoped_refptr<ServiceWorkerRegistration>>&
          registrations);

  bool IsValidGetRegistrationMessage(const GURL& client_url,
                                     std::string* out_error) const;
  bool IsValidGetRegistrationsMessage(std::string* out_error) const;
  bool IsValidGetRegistrationForReadyMessage(std::string* out_error) const;

  // service_manager::mojom::InterfaceProvider:
  // For service worker execution contexts.
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  // Perform common checks that need to run before ContainerHost methods that
  // come from a child process are handled.
  // |scope| is checked if it is allowed to run a service worker.
  // If non-empty, |script_url| is the script associated with the service
  // worker.
  // Returns true if all checks have passed.
  // If anything looks wrong |callback| will run with an error
  // message prefixed by |error_prefix| and |args|, and false is returned.
  template <typename CallbackType, typename... Args>
  bool CanServeContainerHostMethods(CallbackType* callback,
                                    const GURL& scope,
                                    const GURL& script_url,
                                    const char* error_prefix,
                                    Args... args);

  // For service worker execution contexts. Sets the process ID of the service
  // worker execution context.
  void SetWorkerProcessId(int process_id);

  // Unique among all provider hosts.
  const int provider_id_;

  int worker_process_id_ = ChildProcessHost::kInvalidUniqueID;

  // Keyed by registration scope URL length.
  using ServiceWorkerRegistrationMap =
      std::map<size_t, scoped_refptr<ServiceWorkerRegistration>>;
  // Contains all living registrations whose scope this document's URL
  // starts with, used for .ready and claim(). It is empty if
  // IsContextSecureForServiceWorker() is false. See also
  // AddMatchingRegistration().
  ServiceWorkerRegistrationMap matching_registrations_;

  // The ready() promise is only allowed to be created once.
  // |get_ready_callback_| has three states:
  // 1. |get_ready_callback_| is null when ready() has not yet been called.
  // 2. |*get_ready_callback_| is a valid OnceCallback after ready() has been
  //    called and the callback has not yet been run.
  // 3. |*get_ready_callback_| is a null OnceCallback after the callback has
  //    been run.
  std::unique_ptr<GetRegistrationForReadyCallback> get_ready_callback_;

  // For service worker execution contexts. The ServiceWorkerVersion of the
  // service worker this is a provider for.
  const scoped_refptr<ServiceWorkerVersion> running_hosted_version_;

  // |context_| owns |this| but if the context is destroyed and a new one is
  // created, the provider host becomes owned by the new context, while this
  // |context_| is reset to null.
  // TODO(https://crbug.com/877356): Don't support copying context, so this can
  // just be a raw ptr that is never null.
  base::WeakPtr<ServiceWorkerContextCore> context_;

  // |receiver_| keeps the connection to the renderer-side counterpart
  // (content::ServiceWorkerProviderContext). When the connection bound on
  // |receiver_| gets killed from the renderer side, or the bound
  // |ServiceWorkerProviderInfoForStartWorker::host_remote| is otherwise
  // destroyed before being passed to the renderer, this
  // content::ServiceWorkerProviderHost will be destroyed.
  mojo::AssociatedReceiver<blink::mojom::ServiceWorkerContainerHost> receiver_{
      this};

  // Container host receivers other than the original |receiver_|. These include
  // receivers used from (dedicated or shared) worker threads, or from
  // ServiceWorkerSubresourceLoaderFactory.
  mojo::ReceiverSet<blink::mojom::ServiceWorkerContainerHost>
      additional_receivers_;

  // For service worker execution contexts.
  mojo::Binding<service_manager::mojom::InterfaceProvider>
      interface_provider_binding_;
  BrowserInterfaceBrokerImpl<ServiceWorkerProviderHost,
                             const ServiceWorkerVersionInfo&>
      broker_{this};
  mojo::Receiver<blink::mojom::BrowserInterfaceBroker> broker_receiver_{
      &broker_};

  // For service worker clients. The service workers in the chain of redirects
  // during the main resource request for this client. These workers should be
  // updated "soon". See AddServiceWorkerToUpdate() documentation.
  class PendingUpdateVersion;
  base::flat_set<PendingUpdateVersion> versions_to_update_;

  // TODO(https://crbug.com/931087): Make an execution context host (e.g.,
  // RenderFrameHostImpl) own this container host.
  std::unique_ptr<content::ServiceWorkerContainerHost> container_host_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerProviderHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_PROVIDER_HOST_H_
