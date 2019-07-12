// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_WEB_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_LOADER_WEB_WORKER_FETCH_CONTEXT_IMPL_H_

#include <memory>
#include <string>

#include "base/synchronization/waitable_event.h"
#include "content/common/content_export.h"
#include "ipc/ipc_message.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/blob/blob_registry.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_container.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "url/gurl.h"

namespace IPC {
class Message;
}  // namespace IPC

namespace content {

class FrameRequestBlocker;
class ResourceDispatcher;
class ServiceWorkerProviderContext;
class ThreadSafeSender;
class URLLoaderThrottleProvider;
class WebSocketHandshakeThrottleProvider;
struct NavigationResponseOverrideParameters;

// This class is used for fetching resource requests from workers (dedicated
// worker and shared worker). This class is created on the main thread and
// passed to the worker thread. This class is not used for service workers. For
// service workers, ServiceWorkerFetchContextImpl class is used instead.
class CONTENT_EXPORT WebWorkerFetchContextImpl
    : public blink::WebWorkerFetchContext,
      public blink::mojom::ServiceWorkerWorkerClient,
      public blink::mojom::RendererPreferenceWatcher {
 public:
  // Creates a new fetch context for a worker.
  //
  // |provider_context| is used to set up information for using service workers.
  // It can be null if the worker is not allowed to use service workers due to
  // security reasons like sandboxed iframes, insecure origins etc.
  // |loader_factory_info| is used for regular loading by the worker.
  //
  // If the worker is controlled by a service worker, this class makes another
  // loader factory which sends requests to the service worker, and passes
  // |fallback_factory_info| to that factory to use for network fallback.
  //
  // |loader_factory_info| and |fallback_factory_info| are different because
  // |loader_factory_info| can possibly include a default factory like AppCache,
  // while |fallback_factory_info| should not have such a default factory and
  // instead go directly to network for http(s) requests.
  // |fallback_factory_info| might not be simply the direct network factory,
  // because it might additionally support non-NetworkService schemes (e.g.,
  // chrome-extension://).
  static scoped_refptr<WebWorkerFetchContextImpl> Create(
      ServiceWorkerProviderContext* provider_context,
      blink::mojom::RendererPreferences renderer_preferences,
      blink::mojom::RendererPreferenceWatcherRequest watcher_request,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          fallback_factory_info);

  // Clones this fetch context for a nested worker.
  // For non-PlzDedicatedWorker. This will be removed once PlzDedicatedWorker is
  // enabled by default.
  scoped_refptr<WebWorkerFetchContextImpl> CloneForNestedWorkerDeprecated(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);
  // For PlzDedicatedWorker. The cloned fetch context does not inherit some
  // fields (e.g., ServiceWorkerProviderContext) from this fetch context, and
  // instead that takes values passed from the browser process.
  scoped_refptr<WebWorkerFetchContextImpl> CloneForNestedWorker(
      ServiceWorkerProviderContext* service_worker_provider_context,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          fallback_factory_info,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // blink::WebWorkerFetchContext implementation:
  void SetTerminateSyncLoadEvent(base::WaitableEvent*) override;
  void InitializeOnWorkerThread(blink::AcceptLanguagesWatcher*) override;
  blink::WebURLLoaderFactory* GetURLLoaderFactory() override;
  std::unique_ptr<blink::WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) override;
  std::unique_ptr<blink::CodeCacheLoader> CreateCodeCacheLoader() override;
  void WillSendRequest(blink::WebURLRequest&) override;
  blink::mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const override;
  void SetIsOnSubframe(bool) override;
  bool IsOnSubframe() const override;
  blink::WebURL SiteForCookies() const override;
  base::Optional<blink::WebSecurityOrigin> TopFrameOrigin() const override;
  void DidRunContentWithCertificateErrors() override;
  void DidDisplayContentWithCertificateErrors() override;
  void DidRunInsecureContent(const blink::WebSecurityOrigin&,
                             const blink::WebURL& insecure_url) override;
  void SetApplicationCacheHostID(const base::UnguessableToken& id) override;
  void SetSubresourceFilterBuilder(
      std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>) override;
  std::unique_ptr<blink::WebDocumentSubresourceFilter> TakeSubresourceFilter()
      override;
  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;

  // blink::mojom::ServiceWorkerWorkerClient implementation:
  void OnControllerChanged(blink::mojom::ControllerServiceWorkerMode) override;

  // Sets the fetch context status copied from a frame. For dedicated workers,
  // it's copied from the ancestor frame (directly for non-nested workers, or
  // indirectly via its parent worker for nested workers). For shared workers,
  // it's copied from the shadow page.
  void set_controller_service_worker_mode(
      blink::mojom::ControllerServiceWorkerMode mode);
  void set_ancestor_frame_id(int id);
  void set_frame_request_blocker(
      scoped_refptr<FrameRequestBlocker> frame_request_blocker);
  void set_site_for_cookies(const blink::WebURL& site_for_cookies);
  void set_top_frame_origin(const blink::WebSecurityOrigin& top_frame_origin);
  // Sets whether the worker context is a secure context.
  // https://w3c.github.io/webappsec-secure-contexts/
  void set_is_secure_context(bool flag);
  void set_origin_url(const GURL& origin_url);
  void set_client_id(const std::string& client_id);

  // PlzWorker with off-the-main-thread worker script fetch:
  // Sets the response for the worker main script loaded by the browser process.
  void SetResponseOverrideForMainScript(
      std::unique_ptr<NavigationResponseOverrideParameters> response_override);

  using RewriteURLFunction = blink::WebURL (*)(const std::string&, bool);
  static void InstallRewriteURLFunction(RewriteURLFunction rewrite_url);

  blink::WebString GetAcceptLanguages() const override;

 private:
  class Factory;

  // - |service_worker_client_request| is bound to |this| to receive
  //   OnControllerChanged() notifications.
  // - |service_worker_worker_client_registry_info| is a host pointer to
  //   register a new ServiceWorkerWorkerClient, which is needed when creating a
  //   nested worker.
  //
  // Regarding the rest of params, see the comments on Create().
  WebWorkerFetchContextImpl(
      blink::mojom::RendererPreferences renderer_preferences,
      blink::mojom::RendererPreferenceWatcherRequest watcher_request,
      blink::mojom::ServiceWorkerWorkerClientRequest
          service_worker_client_request,
      blink::mojom::ServiceWorkerWorkerClientRegistryPtrInfo
          service_worker_worker_client_registry_info,
      blink::mojom::ServiceWorkerContainerHostPtrInfo
          service_worker_container_host_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          fallback_factory_info,
      std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
      std::unique_ptr<WebSocketHandshakeThrottleProvider>
          websocket_handshake_throttle_provider,
      ThreadSafeSender* thread_safe_sender,
      std::unique_ptr<service_manager::Connector> service_manager_connection);

  ~WebWorkerFetchContextImpl() override;

  scoped_refptr<WebWorkerFetchContextImpl> CloneForNestedWorkerInternal(
      blink::mojom::ServiceWorkerWorkerClientRequest
          service_worker_client_request,
      blink::mojom::ServiceWorkerWorkerClientRegistryPtrInfo
          service_worker_worker_client_registry_ptr_info,
      blink::mojom::ServiceWorkerContainerHostPtrInfo container_host_ptr_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          fallback_factory_info,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  bool Send(IPC::Message* message);

  // Resets the service worker url loader factory of a URLLoaderFactoryImpl
  // which was passed to Blink. The url loader factory is connected to the
  // controller service worker. Sets nullptr if the worker context is not
  // controlled by a service worker.
  void ResetServiceWorkerURLLoaderFactory();

  // Implements blink::mojom::RendererPreferenceWatcher.
  void NotifyUpdate(blink::mojom::RendererPreferencesPtr new_prefs) override;

  // |binding_| and |service_worker_worker_client_registry_| may be null if this
  // context can't use service workers. See comments for Create().
  mojo::Binding<blink::mojom::ServiceWorkerWorkerClient> binding_;
  blink::mojom::ServiceWorkerWorkerClientRegistryPtr
      service_worker_worker_client_registry_;

  // Bound to |this| on the worker thread.
  blink::mojom::ServiceWorkerWorkerClientRequest service_worker_client_request_;
  // Consumed on the worker thread to create
  // |service_worker_worker_client_registry_|.
  blink::mojom::ServiceWorkerWorkerClientRegistryPtrInfo
      service_worker_worker_client_registry_info_;
  // Consumed on the worker thread to create |service_worker_container_host_|.
  blink::mojom::ServiceWorkerContainerHostPtrInfo
      service_worker_container_host_info_;
  // Consumed on the worker thread to create |loader_factory_|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> loader_factory_info_;
  // Consumed on the worker thread to create |fallback_factory_|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> fallback_factory_info_;

  blink::mojom::ControllerServiceWorkerMode controller_service_worker_mode_ =
      blink::mojom::ControllerServiceWorkerMode::kNoController;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  // This can be null if the |provider_context| passed to Create() was null or
  // already being destructed (see
  // ServiceWorkerProviderContext::OnNetworkProviderDestroyed()).
  blink::mojom::ServiceWorkerContainerHostPtr service_worker_container_host_;

  // The Client#id value of the shared worker or dedicated worker (since
  // dedicated workers are not yet service worker clients, it is the parent
  // document's id in that case). Passed to ControllerServiceWorkerConnector.
  std::string client_id_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  // |loader_factory_| is used for regular loading by the worker. In
  // If the worker is controlled by a service worker, it creates a
  // ServiceWorkerSubresourceLoaderFactory instead.
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  // If the worker is controlled by a service worker, it passes this factory to
  // ServiceWorkerSubresourceLoaderFactory to use for network fallback.
  scoped_refptr<network::SharedURLLoaderFactory> fallback_factory_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  scoped_refptr<base::RefCountedData<blink::mojom::BlobRegistryPtr>>
      blob_registry_;

  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  std::unique_ptr<blink::WebDocumentSubresourceFilter::Builder>
      subresource_filter_builder_;
  // For dedicated workers, this is the ancestor frame (the parent frame for
  // non-nested workers, the closest ancestor for nested workers). For shared
  // workers, this is the shadow page.
  bool is_on_sub_frame_ = false;
  int ancestor_frame_id_ = MSG_ROUTING_NONE;
  // Set to non-null if the ancestor frame has an associated RequestBlocker,
  // which blocks requests from this worker too when the ancestor frame is
  // blocked.
  scoped_refptr<FrameRequestBlocker> frame_request_blocker_;
  GURL site_for_cookies_;
  base::Optional<url::Origin> top_frame_origin_;
  bool is_secure_context_ = false;
  GURL origin_url_;
  base::UnguessableToken appcache_host_id_;

  blink::mojom::RendererPreferences renderer_preferences_;

  // |watcher_binding_| and |child_preference_watchers_| are for keeping track
  // of updates in the renderer preferences.
  mojo::Binding<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_binding_;
  // Kept while staring up the worker thread. Valid until
  // InitializeOnWorkerThread().
  blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request_;
  mojo::InterfacePtrSet<blink::mojom::RendererPreferenceWatcher>
      child_preference_watchers_;

  // This is owned by ThreadedMessagingProxyBase on the main thread.
  base::WaitableEvent* terminate_sync_load_event_ = nullptr;

  // The blink::WebURLLoaderFactory which was created and passed to
  // Blink by GetURLLoaderFactory().
  std::unique_ptr<Factory> web_loader_factory_;

  std::unique_ptr<URLLoaderThrottleProvider> throttle_provider_;
  std::unique_ptr<WebSocketHandshakeThrottleProvider>
      websocket_handshake_throttle_provider_;

  std::unique_ptr<service_manager::Connector> service_manager_connection_;

  std::unique_ptr<NavigationResponseOverrideParameters> response_override_;

  blink::AcceptLanguagesWatcher* accept_languages_watcher_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_WEB_WORKER_FETCH_CONTEXT_IMPL_H_
