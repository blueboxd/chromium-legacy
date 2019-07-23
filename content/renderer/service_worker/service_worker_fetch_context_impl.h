// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
#define CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom.h"
#include "third_party/blink/public/platform/web_worker_fetch_context.h"
#include "url/gurl.h"

namespace content {
class ResourceDispatcher;
class URLLoaderThrottleProvider;
class WebSocketHandshakeThrottleProvider;

class ServiceWorkerFetchContextImpl final
    : public blink::WebWorkerFetchContext,
      public blink::mojom::ServiceWorkerSubresourceLoaderUpdater,
      public blink::mojom::RendererPreferenceWatcher {
 public:
  // |url_loader_factory_info| is used for regular loads from the service worker
  // (i.e., Fetch API). It typically goes to network, but it might internally
  // contain non-NetworkService factories for handling non-http(s) URLs like
  // chrome-extension://.
  // |script_loader_factory_info| is used for importScripts() from the service
  // worker when InstalledScriptsManager doesn't have the requested script. It
  // is a ServiceWorkerScriptLoaderFactory, which loads and installs the script.
  ServiceWorkerFetchContextImpl(
      const blink::mojom::RendererPreferences& renderer_preferences,
      const GURL& worker_script_url,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          url_loader_factory_info,
      std::unique_ptr<network::SharedURLLoaderFactoryInfo>
          script_loader_factory_info,
      std::unique_ptr<URLLoaderThrottleProvider> throttle_provider,
      std::unique_ptr<WebSocketHandshakeThrottleProvider>
          websocket_handshake_throttle_provider,
      blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request,
      mojo::PendingReceiver<blink::mojom::ServiceWorkerSubresourceLoaderUpdater>
          pending_subresource_loader_updater);

  // blink::WebWorkerFetchContext implementation:
  void SetTerminateSyncLoadEvent(base::WaitableEvent*) override;
  void InitializeOnWorkerThread(blink::AcceptLanguagesWatcher*) override;
  blink::WebURLLoaderFactory* GetURLLoaderFactory() override;
  std::unique_ptr<blink::WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) override;
  blink::WebURLLoaderFactory* GetScriptLoaderFactory() override;
  void WillSendRequest(blink::WebURLRequest&) override;
  blink::mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const override;
  blink::WebURL SiteForCookies() const override;
  base::Optional<blink::WebSecurityOrigin> TopFrameOrigin() const override;

  std::unique_ptr<blink::WebSocketHandshakeThrottle>
  CreateWebSocketHandshakeThrottle(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner) override;
  blink::WebString GetAcceptLanguages() const override;

 private:
  ~ServiceWorkerFetchContextImpl() override;

  // Implements blink::mojom::ServiceWorkerFetchContext
  void UpdateSubresourceLoaderFactories(
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories) override;

  // Implements blink::mojom::RendererPreferenceWatcher.
  void NotifyUpdate(blink::mojom::RendererPreferencesPtr new_prefs) override;

  blink::mojom::RendererPreferences renderer_preferences_;
  const GURL worker_script_url_;
  // Consumed on the worker thread to create |web_url_loader_factory_|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory_info_;
  // Consumed on the worker thread to create |web_script_loader_factory_|.
  std::unique_ptr<network::SharedURLLoaderFactoryInfo>
      script_loader_factory_info_;

  // Initialized on the worker thread when InitializeOnWorkerThread() is called.
  std::unique_ptr<ResourceDispatcher> resource_dispatcher_;

  // Responsible for regular loads from the service worker (i.e., Fetch API).
  std::unique_ptr<blink::WebURLLoaderFactory> web_url_loader_factory_;
  // Responsible for handling importScripts().
  std::unique_ptr<blink::WebURLLoaderFactory> web_script_loader_factory_;

  std::unique_ptr<URLLoaderThrottleProvider> throttle_provider_;
  std::unique_ptr<WebSocketHandshakeThrottleProvider>
      websocket_handshake_throttle_provider_;

  mojo::Binding<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_binding_;
  mojo::Receiver<blink::mojom::ServiceWorkerSubresourceLoaderUpdater>
      subresource_loader_updater_{this};

  // These mojo objects are kept while starting up the worker thread. Valid
  // until InitializeOnWorkerThread().
  blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request_;
  mojo::PendingReceiver<blink::mojom::ServiceWorkerSubresourceLoaderUpdater>
      pending_subresource_loader_updater_;

  // This is owned by ThreadedMessagingProxyBase on the main thread.
  base::WaitableEvent* terminate_sync_load_event_ = nullptr;

  blink::AcceptLanguagesWatcher* accept_languages_watcher_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_SERVICE_WORKER_FETCH_CONTEXT_IMPL_H_
