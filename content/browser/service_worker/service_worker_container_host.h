// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"
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

  // TODO(https://crbug.com/931087): Rename ServiceWorkerProviderType to
  // ServiceWorkerContainerType.
  ServiceWorkerContainerHost(blink::mojom::ServiceWorkerProviderType type,
                             bool is_parent_frame_secure,
                             ServiceWorkerProviderHost* provider_host,
                             base::WeakPtr<ServiceWorkerContextCore> context);
  ~ServiceWorkerContainerHost();

  ServiceWorkerContainerHost(const ServiceWorkerContainerHost& other) = delete;
  ServiceWorkerContainerHost& operator=(
      const ServiceWorkerContainerHost& other) = delete;
  ServiceWorkerContainerHost(ServiceWorkerContainerHost&& other) = delete;
  ServiceWorkerContainerHost& operator=(ServiceWorkerContainerHost&& other) =
      delete;

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

  // For service worker clients. Sets |url_|, |site_for_cookies_| and
  // |top_frame_origin_|.
  void UpdateUrls(const GURL& url,
                  const GURL& site_for_cookies,
                  const base::Optional<url::Origin>& top_frame_origin);

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
  bool AllowServiceWorker(const GURL& scope,
                          const GURL& script_url,
                          int render_process_id,
                          int frame_id);

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

 private:
  friend class service_worker_object_host_unittest::ServiceWorkerObjectHostTest;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, Unregister);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerJobTest, RegisterDuplicateScript);
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerUpdateJobTest,
                           RegisterWithDifferentUpdateViaCache);
  FRIEND_TEST_ALL_PREFIXES(BackgroundSyncManagerTest,
                           RegisterWithoutLiveSWRegistration);

  void RunExecutionReadyCallbacks();

  const blink::mojom::ServiceWorkerProviderType type_;

  // See comments for the getter functions.
  GURL url_;
  GURL site_for_cookies_;
  base::Optional<url::Origin> top_frame_origin_;

  // |is_parent_frame_secure_| is false if the container host is created for a
  // document whose parent frame is not secure. This doesn't mean the document
  // is necessarily an insecure context, because the document may have a URL
  // whose scheme is granted an exception that allows bypassing the ancestor
  // secure context check. If the container is not created for a document, or
  // the document does not have a parent frame, is_parent_frame_secure_| is
  // true.
  const bool is_parent_frame_secure_;

  // For service worker clients.
  ClientPhase client_phase_ = ClientPhase::kInitial;

  // For service worker clients. Callbacks to run upon transition to
  // kExecutionReady.
  std::vector<ExecutionReadyCallback> execution_ready_callbacks_;

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

  // This provider host owns |this|.
  // TODO(https://crbug.com/931087): Remove dependencies on |provider_host_|.
  ServiceWorkerProviderHost* provider_host_ = nullptr;

  base::WeakPtr<ServiceWorkerContextCore> context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CONTAINER_HOST_H_
