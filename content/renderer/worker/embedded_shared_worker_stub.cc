// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker/embedded_shared_worker_stub.h"

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/possibly_associated_wrapper_shared_url_loader_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/origin_util.h"
#include "content/renderer/loader/child_url_loader_factory_bundle.h"
#include "content/renderer/loader/navigation_response_override_parameters.h"
#include "content/renderer/loader/web_worker_fetch_context_impl.h"
#include "content/renderer/renderer_blink_platform_impl.h"
#include "content/renderer/service_worker/service_worker_provider_context.h"
#include "content/renderer/worker/service_worker_network_provider_for_shared_worker.h"
#include "ipc/ipc_message_macros.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/privacy_preferences.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "third_party/blink/public/platform/interface_provider.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/url_conversion.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/web/web_shared_worker.h"
#include "third_party/blink/public/web/web_shared_worker_client.h"
#include "url/origin.h"

namespace content {

EmbeddedSharedWorkerStub::EmbeddedSharedWorkerStub(
    blink::mojom::SharedWorkerInfoPtr info,
    bool pause_on_start,
    const base::UnguessableToken& devtools_worker_token,
    const blink::mojom::RendererPreferences& renderer_preferences,
    blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request,
    blink::mojom::WorkerContentSettingsProxyPtr content_settings,
    blink::mojom::ServiceWorkerProviderInfoForClientPtr
        service_worker_provider_info,
    const base::UnguessableToken& appcache_host_id,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factory_bundle_info,
    blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
    blink::mojom::SharedWorkerHostPtr host,
    blink::mojom::SharedWorkerRequest request,
    service_manager::mojom::InterfaceProviderPtr interface_provider)
    : binding_(this, std::move(request)),
      host_(std::move(host)),
      name_(info->name),
      url_(info->url),
      renderer_preferences_(renderer_preferences),
      preference_watcher_request_(std::move(preference_watcher_request)) {
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
  DCHECK(subresource_loader_factory_bundle_info);

  if (main_script_load_params) {
    response_override_ =
        std::make_unique<NavigationResponseOverrideParameters>();
    response_override_->url_loader_client_endpoints =
        std::move(main_script_load_params->url_loader_client_endpoints);
    response_override_->response_head = main_script_load_params->response_head;
    response_override_->response_body =
        std::move(main_script_load_params->response_body);
    response_override_->redirect_responses =
        main_script_load_params->redirect_response_heads;
    response_override_->redirect_infos =
        main_script_load_params->redirect_infos;
  }

  impl_ = blink::WebSharedWorker::Create(this, appcache_host_id);
  if (pause_on_start) {
    // Pause worker context when it starts and wait until either DevTools client
    // is attached or explicit resume notification is received.
    impl_->PauseWorkerContextOnStart();
  }

  service_worker_provider_info_ = std::move(service_worker_provider_info);
  controller_info_ = std::move(controller_info);

  // If the network service crashes, then self-destruct so clients don't get
  // stuck with a worker with a broken loader. Self-destruction is effectively
  // the same as the worker's process crashing.
  if (IsOutOfProcessNetworkService()) {
    default_factory_connection_error_handler_holder_.Bind(std::move(
        subresource_loader_factory_bundle_info->pending_default_factory()));
    default_factory_connection_error_handler_holder_->Clone(
        subresource_loader_factory_bundle_info->pending_default_factory()
            .InitWithNewPipeAndPassReceiver());
    default_factory_connection_error_handler_holder_
        .set_connection_error_handler(base::BindOnce(
            &EmbeddedSharedWorkerStub::Terminate, base::Unretained(this)));
  }

  // Initialize the loader factory bundle passed by the browser process.
  DCHECK(!subresource_loader_factory_bundle_);
  subresource_loader_factory_bundle_ =
      base::MakeRefCounted<ChildURLLoaderFactoryBundle>(
          std::make_unique<ChildURLLoaderFactoryBundleInfo>(
              std::move(subresource_loader_factory_bundle_info)));

  impl_->StartWorkerContext(
      url_, blink::WebString::FromUTF8(name_),
      blink::WebString::FromUTF8(info->content_security_policy),
      info->content_security_policy_type, info->creation_address_space,
      devtools_worker_token,
      blink::PrivacyPreferences(renderer_preferences_.enable_do_not_track,
                                renderer_preferences_.enable_referrers),
      subresource_loader_factory_bundle_,
      content_settings.PassInterface().PassHandle(),
      interface_provider.PassInterface().PassHandle());

  // If the host drops its connection, then self-destruct.
  binding_.set_connection_error_handler(base::BindOnce(
      &EmbeddedSharedWorkerStub::Terminate, base::Unretained(this)));
}

EmbeddedSharedWorkerStub::~EmbeddedSharedWorkerStub() {
  // Destruction closes our connection to the host, triggering the host to
  // cleanup and notify clients of this worker going away.
}

void EmbeddedSharedWorkerStub::WorkerReadyForInspection() {
  host_->OnReadyForInspection();
}

void EmbeddedSharedWorkerStub::WorkerScriptLoadFailed() {
  host_->OnScriptLoadFailed();
  pending_channels_.clear();
}

void EmbeddedSharedWorkerStub::WorkerScriptEvaluated(bool success) {
  DCHECK(!running_);
  running_ = true;
  // Process any pending connections.
  for (auto& item : pending_channels_)
    ConnectToChannel(item.first, std::move(item.second));
  pending_channels_.clear();
}

void EmbeddedSharedWorkerStub::CountFeature(blink::mojom::WebFeature feature) {
  host_->OnFeatureUsed(feature);
}

void EmbeddedSharedWorkerStub::WorkerContextClosed() {
  host_->OnContextClosed();
}

void EmbeddedSharedWorkerStub::WorkerContextDestroyed() {
  delete this;
}

std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
EmbeddedSharedWorkerStub::CreateServiceWorkerNetworkProvider() {
  if (blink::features::IsOffMainThreadSharedWorkerScriptFetchEnabled()) {
    // Off-the-main-thread shared worker script fetch:
    // |response_override_| will be passed to WebWorkerFetchContextImpl in
    // CreateWorkerFetchContext() and consumed during off-the-main-thread
    // shared worker script fetch.
    return ServiceWorkerNetworkProviderForSharedWorker::Create(
        std::move(service_worker_provider_info_), std::move(controller_info_),
        subresource_loader_factory_bundle_, IsOriginSecure(url_),
        nullptr /* response_override */);
  }

  // |response_override_| is passed to DocumentLoader and consumed during
  // on-the-main-thread shared worker script fetch.
  return ServiceWorkerNetworkProviderForSharedWorker::Create(
      std::move(service_worker_provider_info_), std::move(controller_info_),
      subresource_loader_factory_bundle_, IsOriginSecure(url_),
      std::move(response_override_));
}

void EmbeddedSharedWorkerStub::WaitForServiceWorkerControllerInfo(
    blink::WebServiceWorkerNetworkProvider* web_network_provider,
    base::OnceClosure callback) {
  ServiceWorkerProviderContext* context =
      static_cast<ServiceWorkerNetworkProviderForSharedWorker*>(
          web_network_provider)
          ->context();
  context->PingContainerHost(std::move(callback));
}

scoped_refptr<blink::WebWorkerFetchContext>
EmbeddedSharedWorkerStub::CreateWorkerFetchContext(
    blink::WebServiceWorkerNetworkProvider* web_network_provider) {
  DCHECK(web_network_provider);
  auto* network_provider =
      static_cast<ServiceWorkerNetworkProviderForSharedWorker*>(
          web_network_provider);

  // Make the factory used for service worker network fallback (that should
  // skip AppCache if it is provided).
  std::unique_ptr<network::SharedURLLoaderFactoryInfo> fallback_factory =
      subresource_loader_factory_bundle_->CloneWithoutAppCacheFactory();

  scoped_refptr<WebWorkerFetchContextImpl> worker_fetch_context =
      WebWorkerFetchContextImpl::Create(
          network_provider->context(), std::move(renderer_preferences_),
          std::move(preference_watcher_request_),
          subresource_loader_factory_bundle_->Clone(),
          std::move(fallback_factory));

  // TODO(horo): To get the correct first_party_to_cookies for the shared
  // worker, we need to check the all documents bounded by the shared worker.
  // (crbug.com/723553)
  // https://tools.ietf.org/html/draft-ietf-httpbis-cookie-same-site-07#section-2.1.2
  worker_fetch_context->set_site_for_cookies(url_);
  // TODO(horo): Currently we treat the worker context as secure if the origin
  // of the shared worker script url is secure. But according to the spec, if
  // the creation context is not secure, we should treat the worker as
  // non-secure. crbug.com/723575
  // https://w3c.github.io/webappsec-secure-contexts/#examples-shared-workers
  worker_fetch_context->set_is_secure_context(IsOriginSecure(url_));
  worker_fetch_context->set_origin_url(url_.GetOrigin());

  if (response_override_) {
    DCHECK(blink::features::IsOffMainThreadSharedWorkerScriptFetchEnabled());
    worker_fetch_context->SetResponseOverrideForMainScript(
        std::move(response_override_));
  }

  return worker_fetch_context;
}

void EmbeddedSharedWorkerStub::ConnectToChannel(
    int connection_request_id,
    blink::MessagePortChannel channel) {
  impl_->Connect(std::move(channel));
  host_->OnConnected(connection_request_id);
}

void EmbeddedSharedWorkerStub::Connect(int connection_request_id,
                                       mojo::ScopedMessagePipeHandle port) {
  blink::MessagePortChannel channel(std::move(port));
  if (running_) {
    ConnectToChannel(connection_request_id, std::move(channel));
  } else {
    // If two documents try to load a SharedWorker at the same time, the
    // mojom::SharedWorker::Connect() for one of the documents can come in
    // before the worker is started. Just queue up the connect and deliver it
    // once the worker starts.
    pending_channels_.emplace_back(connection_request_id, std::move(channel));
  }
}

void EmbeddedSharedWorkerStub::Terminate() {
  // After this we should ignore any IPC for this stub.
  running_ = false;
  impl_->TerminateWorkerContext();
}

void EmbeddedSharedWorkerStub::BindDevToolsAgent(
    blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
    blink::mojom::DevToolsAgentAssociatedRequest request) {
  impl_->BindDevToolsAgent(host.PassHandle(), request.PassHandle());
}

}  // namespace content
