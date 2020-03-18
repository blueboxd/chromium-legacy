// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_
#define CONTENT_RENDERER_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/unguessable_token.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "third_party/blink/public/mojom/browser_interface_broker.mojom-forward.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-forward.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom-forward.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom-forward.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/shared_worker.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom-forward.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/web/web_shared_worker_client.h"
#include "url/gurl.h"

namespace blink {
class WebSharedWorker;
}  // namespace blink

namespace blink {
class MessagePortChannel;
class PendingURLLoaderFactoryBundle;
}  // namespace blink

namespace content {

class ChildURLLoaderFactoryBundle;
struct NavigationResponseOverrideParameters;

// A stub class to receive IPC from browser process and talk to
// blink::WebSharedWorker. Implements blink::WebSharedWorkerClient.
// This class is self-destructed (no one explicitly owns this). It deletes
// itself when WorkerContextDestroyed() is called by blink::WebSharedWorker.
//
// This class owns blink::WebSharedWorker.
class EmbeddedSharedWorkerStub : public blink::WebSharedWorkerClient,
                                 public blink::mojom::SharedWorker {
 public:
  EmbeddedSharedWorkerStub(
      blink::mojom::SharedWorkerInfoPtr info,
      const url::Origin& constructor_origin,
      const std::string& user_agent,
      const blink::UserAgentMetadata& ua_metadata,
      bool pause_on_start,
      const base::UnguessableToken& devtools_worker_token,
      const blink::mojom::RendererPreferences& renderer_preferences,
      mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
          preference_watcher_receiver,
      mojo::PendingRemote<blink::mojom::WorkerContentSettingsProxy>
          content_settings,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr
          service_worker_provider_info,
      const base::UnguessableToken& appcache_host_id,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          pending_subresource_loader_factory_bundle,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      mojo::PendingRemote<blink::mojom::SharedWorkerHost> host,
      mojo::PendingReceiver<blink::mojom::SharedWorker> receiver,
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker);
  ~EmbeddedSharedWorkerStub() override;

  // blink::WebSharedWorkerClient implementation.
  void CountFeature(blink::mojom::WebFeature feature) override;
  void WorkerContextClosed() override;
  void WorkerContextDestroyed() override;
  void WorkerReadyForInspection(
      mojo::ScopedMessagePipeHandle devtools_agent_ptr_info,
      mojo::ScopedMessagePipeHandle devtools_agent_host_request) override;
  void WorkerScriptLoadFailed(const std::string& error_message) override;
  void WorkerScriptEvaluated(bool success) override;
  scoped_refptr<blink::WebWorkerFetchContext> CreateWorkerFetchContext()
      override;

 private:
  // WebSharedWorker will own |channel|.
  void ConnectToChannel(int connection_request_id,
                        blink::MessagePortChannel channel);

  // mojom::SharedWorker methods:
  void Connect(int connection_request_id,
               mojo::ScopedMessagePipeHandle port) override;
  void Terminate() override;

  mojo::Receiver<blink::mojom::SharedWorker> receiver_;
  mojo::Remote<blink::mojom::SharedWorkerHost> host_;
  bool running_ = false;
  GURL url_;
  blink::mojom::RendererPreferences renderer_preferences_;
  // Set on ctor and passed to the fetch context created when
  // CreateWorkerFetchContext() is called.
  mojo::PendingReceiver<blink::mojom::RendererPreferenceWatcher>
      preference_watcher_receiver_;
  std::unique_ptr<blink::WebSharedWorker> impl_;

  using PendingChannel =
      std::pair<int /* connection_request_id */, blink::MessagePortChannel>;
  std::vector<PendingChannel> pending_channels_;

  scoped_refptr<ServiceWorkerProviderContext> service_worker_provider_context_;

  // The factory bundle used for loading subresources for this shared worker.
  scoped_refptr<ChildURLLoaderFactoryBundle> subresource_loader_factory_bundle_;

  // The response override parameters used for taking a resource pre-requested
  // by the browser process.
  std::unique_ptr<NavigationResponseOverrideParameters> response_override_;

  // Out-of-process NetworkService:
  // Detects disconnection from the default factory of the loader factory bundle
  // used by this worker (typically the network service).
  mojo::Remote<network::mojom::URLLoaderFactory>
      default_factory_disconnect_handler_holder_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedSharedWorkerStub);
};

}  // namespace content

#endif  // CONTENT_RENDERER_WORKER_EMBEDDED_SHARED_WORKER_STUB_H_
