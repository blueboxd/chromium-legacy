// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/frame_host/back_forward_cache_metrics.h"
#include "content/common/content_export.h"
#include "content/public/common/child_process_host.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/service_worker/service_worker_status_code.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider_type.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_registration.mojom.h"

namespace service_worker_object_host_unittest {
class ServiceWorkerObjectHostTest;
}

namespace content {

class ServiceWorkerContextCore;
class ServiceWorkerObjectHost;
class ServiceWorkerProviderHost;
class ServiceWorkerRegistration;
class ServiceWorkerRegistrationObjectHost;
class ServiceWorkerVersion;
class WebContents;

// ServiceWorkerContainerHost has a 1:1 correspondence to
// blink::ServiceWorkerContainer (i.e., navigator.serviceWorker) in the renderer
// process.
//
// ServiceWorkerContainerHost manages service worker JavaScript objects and
// service worker registration JavaScript objects, which are represented as
// ServiceWorkerObjectHost and ServiceWorkerRegistrationObjectHost respectively
// in the browser process, associated with the execution context where the
// container lives.
//
// ServiceWorkerObjectHost is tentatively owned by ServiceWorkerProviderHost,
// and has the same lifetime with that.
// TODO(https://crbug.com/931087): Make an execution context host (i.e.,
// RenderFrameHostImpl, DedicatedWorkerHost etc) own this.
//
// TODO(https://crbug.com/931087): Make this inherit
// blink::mojom::ServiceWorkerContainerHost instead of
// ServiceWorkerProviderHost.
//
// TODO(https://crbug.com/931087): Add comments about the thread where this
// class lives, and add sequence checkers to ensure it.
class CONTENT_EXPORT ServiceWorkerContainerHost {
 public:
  using ExecutionReadyCallback = base::OnceClosure;
  using WebContentsGetter = base::RepeatingCallback<WebContents*()>;

  // TODO(https://crbug.com/931087): Rename ServiceWorkerProviderType to
  // ServiceWorkerContainerType.
  ServiceWorkerContainerHost(
      blink::mojom::ServiceWorkerProviderType type,
      bool is_parent_frame_secure,
      int frame_tree_node_id,
      mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
          container_remote,
      ServiceWorkerProviderHost* provider_host,
      base::WeakPtr<ServiceWorkerContextCore> context);
  ~ServiceWorkerContainerHost();

  ServiceWorkerContainerHost(const ServiceWorkerContainerHost& other) = delete;
  ServiceWorkerContainerHost& operator=(
      const ServiceWorkerContainerHost& other) = delete;
  ServiceWorkerContainerHost(ServiceWorkerContainerHost&& other) = delete;
  ServiceWorkerContainerHost& operator=(ServiceWorkerContainerHost&& other) =
      delete;

  // TODO(https://crbug.com/931087): EnsureControllerServiceWorker should be
  // implemented as blink::mojom::ServiceWorkerContainerHost override.
  void EnsureControllerServiceWorker(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::mojom::ControllerServiceWorkerPurpose purpose);

  // TODO(https://crbug.com/931087): OnSkippedWaiting should be implemented as
  // ServiceWorkerRegistration::Listener override.
  void OnSkippedWaiting(ServiceWorkerRegistration* registration);

  // Dispatches message event to the client (document, dedicated worker when
  // PlzDedicatedWorker is enabled, or shared worker).
  void PostMessageToClient(ServiceWorkerVersion* version,
                           blink::TransferableMessage message);

  // Notifies the client that its controller used a feature, for UseCounter
  // purposes. This can only be called if IsContainerForClient() is true.
  void CountFeature(blink::mojom::WebFeature feature);

  // Sends information about the controller to the container of the service
  // worker clients in the renderer. If |notify_controllerchange| is true,
  // instructs the renderer to dispatch a 'controllerchange' event.
  void SendSetControllerServiceWorker(bool notify_controllerchange);

  // Called when this container host's controller has been terminated and doomed
  // due to an exceptional condition like it could no longer be read from the
  // script cache.
  void NotifyControllerLost();

  // Returns an object info representing |registration|. The object info holds a
  // Mojo connection to the ServiceWorkerRegistrationObjectHost for the
  // |registration| to ensure the host stays alive while the object info is
  // alive. A new ServiceWorkerRegistrationObjectHost instance is created if one
  // can not be found in |registration_object_hosts_|.
  //
  // NOTE: The registration object info should be sent over Mojo in the same
  // task with calling this method. Otherwise, some Mojo calls to
  // blink::mojom::ServiceWorkerRegistrationObject or
  // blink::mojom::ServiceWorkerObject may happen before establishing the
  // connections, and they'll end up with crashes.
  blink::mojom::ServiceWorkerRegistrationObjectInfoPtr
  CreateServiceWorkerRegistrationObjectInfo(
      scoped_refptr<ServiceWorkerRegistration> registration);

  // Removes the ServiceWorkerRegistrationObjectHost corresponding to
  // |registration_id|.
  void RemoveServiceWorkerRegistrationObjectHost(int64_t registration_id);

  // For the container hosted on ServiceWorkerGlobalScope.
  // Returns an object info representing |self.serviceWorker|. The object
  // info holds a Mojo connection to the ServiceWorkerObjectHost for the
  // |serviceWorker| to ensure the host stays alive while the object info is
  // alive. See documentation.
  blink::mojom::ServiceWorkerObjectInfoPtr CreateServiceWorkerObjectInfoToSend(
      scoped_refptr<ServiceWorkerVersion> version);

  // Returns a ServiceWorkerObjectHost instance for |version| for this provider
  // host. A new instance is created if one does not already exist.
  // ServiceWorkerObjectHost will have an ownership of the |version|.
  base::WeakPtr<ServiceWorkerObjectHost> GetOrCreateServiceWorkerObjectHost(
      scoped_refptr<ServiceWorkerVersion> version);

  // Removes the ServiceWorkerObjectHost corresponding to |version_id|.
  void RemoveServiceWorkerObjectHost(int64_t version_id);

  bool IsContainerForServiceWorker() const;
  bool IsContainerForClient() const;

  blink::mojom::ServiceWorkerProviderType type() const { return type_; }

  // Can only be called when IsContainerForClient() is true.
  blink::mojom::ServiceWorkerClientType client_type() const;

  // For service worker window clients. Called when the navigation is ready to
  // commit. Updates this host with information about the frame committed to.
  // After this is called, is_response_committed() and is_execution_ready()
  // return true.
  void OnBeginNavigationCommit(
      int container_process_id,
      int container_frame_id,
      network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy);

  // For service worker clients that are shared workers or dedicated workers.
  // Called when the web worker main script resource has finished loading.
  // Updates this host with information about the worker.
  // After this is called, is_response_committed() and is_execution_ready()
  // return true.
  void CompleteWebWorkerPreparation(
      network::mojom::CrossOriginEmbedderPolicy cross_origin_embedder_policy);

  // For service worker clients. Sets |url_|, |site_for_cookies_| and
  // |top_frame_origin_|.
  void UpdateUrls(const GURL& url,
                  const GURL& site_for_cookies,
                  const base::Optional<url::Origin>& top_frame_origin);

  // For service worker clients. Makes this client be controlled by
  // |registration|'s active worker, or makes this client be not
  // controlled if |registration| is null. If |notify_controllerchange| is true,
  // instructs the renderer to dispatch a 'controllerchange' event.
  void SetControllerRegistration(
      scoped_refptr<ServiceWorkerRegistration> controller_registration,
      bool notify_controllerchange);

  // For service worker clients. Similar to EnsureControllerServiceWorker, but
  // this returns a bound Mojo ptr which is supposed to be sent to clients. The
  // controller ptr passed to the clients will be used to intercept requests
  // from them.
  // It is invalid to call this when controller_ is null.
  //
  // This method can be called in one of the following cases:
  //
  // - During navigation, right after a request handler for the main resource
  //   has found the matching registration and has started the worker.
  // - When a controller is updated by UpdateController() (e.g.
  //   by OnSkippedWaiting() or SetControllerRegistration()).
  //   In some cases the controller worker may not be started yet.
  //
  // This may return nullptr if the controller service worker does not have a
  // fetch handler, i.e. when the renderer does not need the controller ptr.
  //
  // WARNING:
  // Unlike EnsureControllerServiceWorker, this method doesn't guarantee that
  // the controller worker is running because this method can be called in some
  // situations where the worker isn't running yet. When the returned ptr is
  // stored somewhere and intended to use later, clients need to make sure
  // that the worker is eventually started to use the ptr.
  // Currently all the callsites do this, i.e. they start the worker before
  // or after calling this, but there's no mechanism to prevent future breakage.
  // TODO(crbug.com/827935): Figure out a way to prevent misuse of this method.
  // TODO(crbug.com/827935): Make sure the connection error handler fires in
  // ControllerServiceWorkerConnector (so that it can correctly call
  // EnsureControllerServiceWorker later) if the worker gets killed before
  // events are dispatched.
  //
  // TODO(kinuko): revisit this if we start to use the ControllerServiceWorker
  // for posting messages.
  // TODO(hayato): Return PendingRemote, instead of Remote. Binding to Remote
  // as late as possible is more idiomatic for new Mojo types.
  mojo::Remote<blink::mojom::ControllerServiceWorker>
  GetRemoteControllerServiceWorker();

  // |registration| claims the client (document, dedicated worker when
  // PlzDedicatedWorker is enabled, or shared worker) to be controlled.
  void ClaimedByRegistration(
      scoped_refptr<ServiceWorkerRegistration> registration);

  // The URL of this context. For service worker clients, this is the document
  // URL (for documents) or script URL (for workers). For service worker
  // execution contexts, this is the script URL.
  //
  // For clients, url() may be empty if loading has not started, or our custom
  // loading handler didn't see the load (because e.g. another handler did
  // first, or the initial request URL was such that
  // OriginCanAccessServiceWorkers returned false).
  //
  // The URL may also change on redirects during loading. Once
  // is_response_committed() is true, the URL should no longer change.
  const GURL& url() const { return url_; }

  // The URL representing the site_for_cookies for this context. See
  // |URLRequest::site_for_cookies()| for details.
  // For service worker execution contexts, site_for_cookies() always
  // returns the service worker script URL.
  const GURL& site_for_cookies() const { return site_for_cookies_; }

  // The URL representing the first-party site for this context.
  // For service worker execution contexts, top_frame_origin() always
  // returns the origin of the service worker script URL.
  // For shared worker it is the origin of the document that created the worker.
  // For dedicated worker it is the top-frame origin of the document that owns
  // the worker.
  base::Optional<url::Origin> top_frame_origin() const {
    return top_frame_origin_;
  }

  // Calls ContentBrowserClient::AllowServiceWorker(). Returns true if content
  // settings allows service workers to run at |scope|. If this container is for
  // a window client, the check involves the topmost frame url as well as
  // |scope|, and may display tab-level UI.
  // If non-empty, |script_url| is the script the service worker will run.
  bool AllowServiceWorker(const GURL& scope, const GURL& script_url);

  // Returns whether this provider host is secure enough to have a service
  // worker controller.
  // Analogous to Blink's Document::IsSecureContext. Because of how service
  // worker intercepts main resource requests, this check must be done
  // browser-side once the URL is known (see comments in
  // ServiceWorkerNetworkProviderForFrame::Create). This function uses
  // |url_| and |is_parent_frame_secure_| to determine context security, so they
  // must be set properly before calling this function.
  bool IsContextSecureForServiceWorker() const;

  // For service worker clients. True if the response for the main resource load
  // was committed to the renderer. When this is false, the client's URL may
  // still change due to redirects.
  bool is_response_committed() const;

  // For service worker clients. |callback| is called when this client becomes
  // execution ready or if it is destroyed first.
  void AddExecutionReadyCallback(ExecutionReadyCallback callback);

  // For service worker clients. True if the client is execution ready and
  // therefore can be exposed to JavaScript. Execution ready implies response
  // committed.
  // https://html.spec.whatwg.org/multipage/webappapis.html#concept-environment-execution-ready-flag
  bool is_execution_ready() const;

  // TODO(https://crbug.com/931087): Remove this getter and |provider_host_|.
  ServiceWorkerProviderHost* provider_host() { return provider_host_; }

  // For service worker clients. The flow is kInitial -> kResponseCommitted ->
  // kExecutionReady.
  //
  // - kInitial: The initial phase.
  // - kResponseCommitted: The response for the main resource has been
  //   committed to the renderer. This client's URL should no longer change.
  // - kExecutionReady: This client can be exposed to JavaScript as a Client
  //   object.
  // TODO(https://crbug.com/931087): Move this enum into the private section.
  enum class ClientPhase { kInitial, kResponseCommitted, kExecutionReady };

  // Sets |execution_ready_| and runs execution ready callbacks.
  // TODO(https://crbug.com/931087): Move this function into the private
  // section.
  void SetExecutionReady();

  // TODO(https://crbug.com/931087): Move this function into the private
  // section.
  void TransitionToClientPhase(ClientPhase new_phase);

  // For service worker clients. Returns false if it's not yet time to send the
  // renderer information about the controller. Basically returns false if this
  // client is still loading so due to potential redirects the initial
  // controller has not yet been decided.
  // TODO(https://crbug.com/931087): Move this function into the private
  // section.
  bool IsControllerDecided() const;

  const base::UnguessableToken& fetch_request_window_id() const {
    return fetch_request_window_id_;
  }

  void SetContainerProcessId(int process_id);

  base::TimeTicks create_time() const { return create_time_; }
  int process_id() const { return process_id_; }
  int frame_id() const { return frame_id_; }
  int frame_tree_node_id() const { return frame_tree_node_id_; }

  const WebContentsGetter& web_contents_getter() const {
    return web_contents_getter_;
  }

  const std::string& client_uuid() const { return client_uuid_; }

  // For service worker clients. Describes whether the client has a controller
  // and if it has a fetch event handler.
  blink::mojom::ControllerServiceWorkerMode GetControllerMode() const;

  // For service worker clients. Returns this client's controller.
  ServiceWorkerVersion* controller() const;

  // For service worker clients. Returns this client's controller's
  // registration.
  ServiceWorkerRegistration* controller_registration() const;

  // BackForwardCache:
  // For service worker clients that are windows.
  bool IsInBackForwardCache() const;
  void EvictFromBackForwardCache(
      BackForwardCacheMetrics::NotRestoredReason reason);
  // Called when this container host's frame goes into BackForwardCache.
  void OnEnterBackForwardCache();
  // Called when a frame gets restored from BackForwardCache. Note that a
  // BackForwardCached frame can be deleted while in the cache but in this case
  // OnRestoreFromBackForwardCache will not be called.
  void OnRestoreFromBackForwardCache();

  void EnterBackForwardCacheForTesting() { is_in_back_forward_cache_ = true; }
  void LeaveBackForwardCacheForTesting() { is_in_back_forward_cache_ = false; }

 private:
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, Unregister);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, RegisterDuplicateScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerUpdateJobTest,
                           RegisterWithDifferentUpdateViaCache);
  FRIEND_TEST_ALL_PREFIXES(BackgroundSyncManagerTest,
                           RegisterWithoutLiveSWRegistration);

  void RunExecutionReadyCallbacks();

  // Sets the controller to |controller_registration_->active_version()| or null
  // if there is no associated registration.
  //
  // If |notify_controllerchange| is true, instructs the renderer to dispatch a
  // 'controller' change event.
  void UpdateController(bool notify_controllerchange);

#if DCHECK_IS_ON()
  void CheckControllerConsistency(bool should_crash) const;
#endif  // DCHECK_IS_ON()

  // Callback for ServiceWorkerVersion::RunAfterStartWorker()
  void StartControllerComplete(
      mojo::PendingReceiver<blink::mojom::ControllerServiceWorker> receiver,
      blink::ServiceWorkerStatusCode status);

  const blink::mojom::ServiceWorkerProviderType type_;

  // See comments for the getter functions.
  GURL url_;
  GURL site_for_cookies_;
  base::Optional<url::Origin> top_frame_origin_;

  // For window clients. A token used internally to identify this context in
  // requests. Corresponds to the Fetch specification's concept of a request's
  // associated window: https://fetch.spec.whatwg.org/#concept-request-window
  // This gets reset on redirects, unlike |client_uuid_|.
  //
  // TODO(falken): Consider using this for |client_uuid_| as well. We can't
  // right now because this gets reset on redirects, and potentially sites rely
  // on the GUID format.
  base::UnguessableToken fetch_request_window_id_;

  // The time when the container host is created.
  const base::TimeTicks create_time_;

  // The identifier of the process where the container lives.
  int process_id_ = ChildProcessHost::kInvalidUniqueID;

  // The window's RenderFrame id, if this is a service worker window client.
  // Otherwise, |MSG_ROUTING_NONE|.
  int frame_id_ = MSG_ROUTING_NONE;

  // |is_parent_frame_secure_| is false if the container host is created for a
  // document whose parent frame is not secure. This doesn't mean the document
  // is necessarily an insecure context, because the document may have a URL
  // whose scheme is granted an exception that allows bypassing the ancestor
  // secure context check. If the container is not created for a document, or
  // the document does not have a parent frame, is_parent_frame_secure_| is
  // true.
  const bool is_parent_frame_secure_;

  // FrameTreeNode id if this is a service worker window client.
  // Otherwise, |FrameTreeNode::kFrameTreeNodeInvalidId|.
  const int frame_tree_node_id_;

  // Only set when this object is pre-created for a navigation. It indicates the
  // tab where the navigation occurs. Otherwise, a null callback.
  const WebContentsGetter web_contents_getter_;

  // A GUID that is web-exposed as FetchEvent.clientId.
  std::string client_uuid_;

  // For service worker clients.
  ClientPhase client_phase_ = ClientPhase::kInitial;

  // For service worker clients. The embedder policy of the client. Set on
  // response commit.
  base::Optional<network::mojom::CrossOriginEmbedderPolicy>
      cross_origin_embedder_policy_;

  // TODO(yuzus): This bit will be unnecessary once ServiceWorkerContainerHost
  // and RenderFrameHost have the same lifetime.
  bool is_in_back_forward_cache_ = false;

  // For service worker clients. Callbacks to run upon transition to
  // kExecutionReady.
  std::vector<ExecutionReadyCallback> execution_ready_callbacks_;

  // For service worker clients. The controller service worker (i.e.,
  // ServiceWorkerContainer#controller) and its registration. The controller is
  // typically the same as the registration's active version, but during
  // algorithms such as the update, skipWaiting(), and claim() steps, the active
  // version and controller may temporarily differ. For example, to perform
  // skipWaiting(), the registration's active version is updated first and then
  // the container host's controller is updated to match it.
  scoped_refptr<ServiceWorkerVersion> controller_;
  scoped_refptr<ServiceWorkerRegistration> controller_registration_;

  // Contains all ServiceWorkerRegistrationObjectHost instances corresponding to
  // the service worker registration JavaScript objects for the hosted execution
  // context (service worker global scope or service worker client) in the
  // renderer process.
  std::map<int64_t /* registration_id */,
           std::unique_ptr<ServiceWorkerRegistrationObjectHost>>
      registration_object_hosts_;

  // Contains all ServiceWorkerObjectHost instances corresponding to
  // the service worker JavaScript objects for the hosted execution
  // context (service worker global scope or service worker client) in the
  // renderer process.
  std::map<int64_t /* version_id */, std::unique_ptr<ServiceWorkerObjectHost>>
      service_worker_object_hosts_;

  // Mojo endpoint which will be be sent to the service worker just before
  // the response is committed, where |cross_origin_embedder_policy_| is ready.
  // We need to store this here because navigation code depends on having a
  // mojo::Remote<ControllerServiceWorker> for making a SubresourceLoaderParams,
  // which is created before the response header is ready.
  mojo::PendingReceiver<blink::mojom::ControllerServiceWorker>
      pending_controller_receiver_;

  // |container_| is the remote renderer-side ServiceWorkerContainer that |this|
  // is hosting.
  mojo::AssociatedRemote<blink::mojom::ServiceWorkerContainer> container_;

  // This provider host owns |this|.
  // TODO(https://crbug.com/931087): Remove dependencies on |provider_host_|.
  ServiceWorkerProviderHost* provider_host_ = nullptr;

  base::WeakPtr<ServiceWorkerContextCore> context_;

  base::WeakPtrFactory<ServiceWorkerContainerHost> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_
