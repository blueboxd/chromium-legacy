// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_host.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/unguessable_token.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/shared_worker_content_settings_proxy_impl.h"
#include "content/browser/worker_host/shared_worker_instance.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_client.h"
#include "net/base/network_isolation_key.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/renderer_preference_watcher.mojom.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom.h"

namespace content {
namespace {

SharedWorkerHost::CreateNetworkFactoryCallback&
GetCreateNetworkFactoryCallbackForSharedWorker() {
  static base::NoDestructor<SharedWorkerHost::CreateNetworkFactoryCallback>
      s_callback;
  return *s_callback;
}

void AllowFileSystemOnIOThreadResponse(base::OnceCallback<void(bool)> callback,
                                       bool result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  base::PostTaskWithTraits(FROM_HERE, {BrowserThread::UI},
                           base::BindOnce(std::move(callback), result));
}

void AllowFileSystemOnIOThread(const GURL& url,
                               ResourceContext* resource_context,
                               std::vector<GlobalFrameRoutingId> render_frames,
                               base::OnceCallback<void(bool)> callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  GetContentClient()->browser()->AllowWorkerFileSystem(
      url, resource_context, render_frames,
      base::Bind(&AllowFileSystemOnIOThreadResponse, base::Passed(&callback)));
}

bool AllowIndexedDBOnIOThread(const GURL& url,
                              ResourceContext* resource_context,
                              std::vector<GlobalFrameRoutingId> render_frames) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return GetContentClient()->browser()->AllowWorkerIndexedDB(
      url, resource_context, render_frames);
}

bool AllowCacheStorageOnIOThread(
    const GURL& url,
    ResourceContext* resource_context,
    std::vector<GlobalFrameRoutingId> render_frames) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return GetContentClient()->browser()->AllowWorkerCacheStorage(
      url, resource_context, render_frames);
}

}  // namespace

// RAII helper class for talking to SharedWorkerDevToolsManager.
class SharedWorkerHost::ScopedDevToolsHandle {
 public:
  ScopedDevToolsHandle(SharedWorkerHost* owner,
                       bool* out_pause_on_start,
                       base::UnguessableToken* out_devtools_worker_token)
      : owner_(owner) {
    SharedWorkerDevToolsManager::GetInstance()->WorkerCreated(
        owner, out_pause_on_start, out_devtools_worker_token);
  }

  ~ScopedDevToolsHandle() {
    SharedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(owner_);
  }

  void WorkerReadyForInspection() {
    SharedWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
        owner_);
  }

 private:
  SharedWorkerHost* owner_;
  DISALLOW_COPY_AND_ASSIGN(ScopedDevToolsHandle);
};

SharedWorkerHost::SharedWorkerHost(
    SharedWorkerServiceImpl* service,
    std::unique_ptr<SharedWorkerInstance> instance,
    int worker_process_id)
    : binding_(this),
      service_(service),
      instance_(std::move(instance)),
      worker_process_id_(worker_process_id),
      next_connection_request_id_(1),
      interface_provider_binding_(this) {
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
  DCHECK(instance_);
  // Set up the worker interface request. This is needed first in either
  // AddClient() or Start(). AddClient() can sometimes be called before Start()
  // when two clients call new SharedWorker() at around the same time.
  worker_request_ = mojo::MakeRequest(&worker_);

  // Keep the renderer process alive that will be hosting the shared worker.
  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  DCHECK(!IsShuttingDown(worker_process_host));
  worker_process_host->IncrementKeepAliveRefCount();
}

SharedWorkerHost::~SharedWorkerHost() {
  switch (phase_) {
    case Phase::kInitial:
      // Tell clients that this worker failed to start. This is only needed in
      // kInitial. Once in kStarted, the worker in the renderer would alert this
      // host if script loading failed.
      for (const ClientInfo& info : clients_)
        info.client->OnScriptLoadFailed();
      break;
    case Phase::kStarted:
    case Phase::kClosed:
    case Phase::kTerminationSent:
    case Phase::kTerminationSentAndClosed:
      break;
  }

  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  if (!IsShuttingDown(worker_process_host))
    worker_process_host->DecrementKeepAliveRefCount();
}

// static
void SharedWorkerHost::SetNetworkFactoryForTesting(
    const CreateNetworkFactoryCallback& create_network_factory_callback) {
  DCHECK(!BrowserThread::IsThreadInitialized(BrowserThread::UI) ||
         BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(create_network_factory_callback.is_null() ||
         GetCreateNetworkFactoryCallbackForSharedWorker().is_null())
      << "It is not expected that this is called with non-null callback when "
      << "another overriding callback is already set.";
  GetCreateNetworkFactoryCallbackForSharedWorker() =
      create_network_factory_callback;
}

void SharedWorkerHost::Start(
    blink::mojom::SharedWorkerFactoryPtr factory,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    base::WeakPtr<ServiceWorkerObjectHost>
        controller_service_worker_object_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(main_script_load_params);
  DCHECK(subresource_loader_factories);
  DCHECK(!subresource_loader_factories->pending_default_factory());

  AdvanceTo(Phase::kStarted);

  blink::mojom::SharedWorkerInfoPtr info(blink::mojom::SharedWorkerInfo::New(
      instance_->url(), instance_->name(), instance_->content_security_policy(),
      instance_->content_security_policy_type(),
      instance_->creation_address_space()));

  // Register with DevTools.
  bool pause_on_start;
  base::UnguessableToken devtools_worker_token;
  devtools_handle_ = std::make_unique<ScopedDevToolsHandle>(
      this, &pause_on_start, &devtools_worker_token);

  auto renderer_preferences = blink::mojom::RendererPreferences::New();
  GetContentClient()->browser()->UpdateRendererPreferencesForWorker(
      RenderProcessHost::FromID(worker_process_id_)->GetBrowserContext(),
      renderer_preferences.get());

  // Create a RendererPreferenceWatcher to observe updates in the preferences.
  blink::mojom::RendererPreferenceWatcherPtr watcher_ptr;
  blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request =
      mojo::MakeRequest(&watcher_ptr);
  GetContentClient()->browser()->RegisterRendererPreferenceWatcher(
      RenderProcessHost::FromID(worker_process_id_)->GetBrowserContext(),
      std::move(watcher_ptr));

  // Set up content settings interface.
  blink::mojom::WorkerContentSettingsProxyPtr content_settings;
  content_settings_ = std::make_unique<SharedWorkerContentSettingsProxyImpl>(
      instance_->url(), this, mojo::MakeRequest(&content_settings));

  // Set up host interface.
  blink::mojom::SharedWorkerHostPtr host;
  binding_.Bind(mojo::MakeRequest(&host));

  // Set up interface provider interface.
  service_manager::mojom::InterfaceProviderPtr interface_provider;
  interface_provider_binding_.Bind(FilterRendererExposedInterfaces(
      blink::mojom::kNavigation_SharedWorkerSpec, worker_process_id_,
      mojo::MakeRequest(&interface_provider)));

  // Set the default factory to the bundle for subresource loading to pass to
  // the renderer.
  mojo::PendingRemote<network::mojom::URLLoaderFactory> pending_default_factory;
  CreateNetworkFactory(
      pending_default_factory.InitWithNewPipeAndPassReceiver());
  subresource_loader_factories->pending_default_factory() =
      std::move(pending_default_factory);

  // Prepare the controller service worker info to pass to the renderer.
  // |object_info| can be nullptr when the service worker context or the service
  // worker version is gone during shared worker startup.
  blink::mojom::ServiceWorkerObjectAssociatedPtrInfo
      service_worker_remote_object;
  blink::mojom::ServiceWorkerState service_worker_sent_state;
  if (controller && controller->object_info) {
    controller->object_info->request =
        mojo::MakeRequest(&service_worker_remote_object);
    service_worker_sent_state = controller->object_info->state;
  }

  // Send the CreateSharedWorker message.
  factory_ = std::move(factory);
  factory_->CreateSharedWorker(
      std::move(info), pause_on_start, devtools_worker_token,
      std::move(renderer_preferences), std::move(preference_watcher_request),
      std::move(content_settings), service_worker_handle_->TakeProviderInfo(),
      appcache_handle_
          ? base::make_optional(appcache_handle_->appcache_host_id())
          : base::nullopt,
      std::move(main_script_load_params),
      std::move(subresource_loader_factories), std::move(controller),
      std::move(host), std::move(worker_request_),
      std::move(interface_provider));

  // |service_worker_remote_object| is an associated interface ptr, so calls
  // can't be made on it until its request endpoint is sent. Now that the
  // request endpoint was sent, it can be used, so add it to
  // ServiceWorkerObjectHost.
  if (service_worker_remote_object.is_valid()) {
    base::PostTaskWithTraits(
        FROM_HERE, {BrowserThread::IO},
        base::BindOnce(
            &ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState,
            controller_service_worker_object_host,
            std::move(service_worker_remote_object),
            service_worker_sent_state));
  }

  // Monitor the lifetime of the worker.
  worker_.set_connection_error_handler(base::BindOnce(
      &SharedWorkerHost::OnWorkerConnectionLost, weak_factory_.GetWeakPtr()));
}

//  This is similar to
//  RenderFrameHostImpl::CreateNetworkServiceDefaultFactoryAndObserve, but this
//  host doesn't observe network service crashes. Instead, the renderer detects
//  the connection error and terminates the worker.
void SharedWorkerHost::CreateNetworkFactory(
    network::mojom::URLLoaderFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  url::Origin origin = instance_->constructor_origin();
  network::mojom::TrustedURLLoaderHeaderClientPtrInfo no_header_client;

  if (GetCreateNetworkFactoryCallbackForSharedWorker().is_null()) {
    worker_process_host->CreateURLLoaderFactory(
        origin, nullptr /* preferences */,
        net::NetworkIsolationKey(origin, origin), std::move(no_header_client),
        std::move(request));
  } else {
    network::mojom::URLLoaderFactoryPtr original_factory;
    worker_process_host->CreateURLLoaderFactory(
        origin, nullptr /* preferences */,
        net::NetworkIsolationKey(origin, origin), std::move(no_header_client),
        mojo::MakeRequest(&original_factory));
    GetCreateNetworkFactoryCallbackForSharedWorker().Run(
        std::move(request), worker_process_id_,
        original_factory.PassInterface());
  }
}

void SharedWorkerHost::AllowFileSystem(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AllowFileSystemOnIOThread, url,
                     RenderProcessHost::FromID(worker_process_id_)
                         ->GetBrowserContext()
                         ->GetResourceContext(),
                     GetRenderFrameIDsForWorker(), std::move(callback)));
}

void SharedWorkerHost::AllowIndexedDB(const GURL& url,
                                      base::OnceCallback<void(bool)> callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AllowIndexedDBOnIOThread, url,
                     RenderProcessHost::FromID(worker_process_id_)
                         ->GetBrowserContext()
                         ->GetResourceContext(),
                     GetRenderFrameIDsForWorker()),
      std::move(callback));
}

void SharedWorkerHost::AllowCacheStorage(
    const GURL& url,
    base::OnceCallback<void(bool)> callback) {
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&AllowCacheStorageOnIOThread, url,
                     RenderProcessHost::FromID(worker_process_id_)
                         ->GetBrowserContext()
                         ->GetResourceContext(),
                     GetRenderFrameIDsForWorker()),
      std::move(callback));
}

void SharedWorkerHost::TerminateWorker() {
  switch (phase_) {
    case Phase::kInitial:
      // The host is being asked to terminate the worker before it started.
      // Tell clients that this worker failed to start.
      for (const ClientInfo& info : clients_)
        info.client->OnScriptLoadFailed();
      // Tell the caller it terminated, so the caller doesn't wait forever.
      AdvanceTo(Phase::kTerminationSentAndClosed);
      OnWorkerConnectionLost();
      // |this| is destroyed here.
      return;
    case Phase::kStarted:
      AdvanceTo(Phase::kTerminationSent);
      break;
    case Phase::kClosed:
      AdvanceTo(Phase::kTerminationSentAndClosed);
      break;
    case Phase::kTerminationSent:
    case Phase::kTerminationSentAndClosed:
      // Termination was already sent. TerminateWorker can be called twice in
      // tests while cleaning up all the workers.
      return;
  }

  devtools_handle_.reset();
  worker_->Terminate();
  // Now, we wait to observe OnWorkerConnectionLost.
}

void SharedWorkerHost::AdvanceTo(Phase phase) {
  switch (phase_) {
    case Phase::kInitial:
      DCHECK(phase == Phase::kStarted ||
             phase == Phase::kTerminationSentAndClosed);
      break;
    case Phase::kStarted:
      DCHECK(phase == Phase::kClosed || phase == Phase::kTerminationSent);
      break;
    case Phase::kClosed:
      DCHECK(phase == Phase::kTerminationSentAndClosed);
      break;
    case Phase::kTerminationSent:
      DCHECK(phase == Phase::kTerminationSentAndClosed);
      break;
    case Phase::kTerminationSentAndClosed:
      NOTREACHED();
      break;
  }
  phase_ = phase;
}

SharedWorkerHost::ClientInfo::ClientInfo(
    blink::mojom::SharedWorkerClientPtr client,
    int connection_request_id,
    int client_process_id,
    int frame_id)
    : client(std::move(client)),
      connection_request_id(connection_request_id),
      client_process_id(client_process_id),
      frame_id(frame_id) {}

SharedWorkerHost::ClientInfo::~ClientInfo() {}

void SharedWorkerHost::OnConnected(int connection_request_id) {
  if (!instance_)
    return;
  for (const ClientInfo& info : clients_) {
    if (info.connection_request_id != connection_request_id)
      continue;
    info.client->OnConnected(std::vector<blink::mojom::WebFeature>(
        used_features_.begin(), used_features_.end()));
    return;
  }
}

void SharedWorkerHost::OnContextClosed() {
  devtools_handle_.reset();

  // Mark as closed - this will stop any further messages from being sent to the
  // worker (messages can still be sent from the worker, for exception
  // reporting, etc).
  switch (phase_) {
    case Phase::kInitial:
      // Not possible: there is no Mojo connection on which OnContextClosed can
      // be called.
      NOTREACHED();
      break;
    case Phase::kStarted:
      AdvanceTo(Phase::kClosed);
      break;
    case Phase::kTerminationSent:
      AdvanceTo(Phase::kTerminationSentAndClosed);
      break;
    case Phase::kClosed:
    case Phase::kTerminationSentAndClosed:
      // Already closed, just ignore.
      break;
  }
}

void SharedWorkerHost::OnReadyForInspection() {
  if (devtools_handle_)
    devtools_handle_->WorkerReadyForInspection();
}

void SharedWorkerHost::OnScriptLoadFailed() {
  for (const ClientInfo& info : clients_)
    info.client->OnScriptLoadFailed();
}

void SharedWorkerHost::OnFeatureUsed(blink::mojom::WebFeature feature) {
  // Avoid reporting a feature more than once, and enable any new clients to
  // observe features that were historically used.
  if (!used_features_.insert(feature).second)
    return;
  for (const ClientInfo& info : clients_)
    info.client->OnFeatureUsed(feature);
}

std::vector<GlobalFrameRoutingId>
SharedWorkerHost::GetRenderFrameIDsForWorker() {
  std::vector<GlobalFrameRoutingId> result;
  result.reserve(clients_.size());
  for (const ClientInfo& info : clients_) {
    result.push_back(
        GlobalFrameRoutingId(info.client_process_id, info.frame_id));
  }
  return result;
}

bool SharedWorkerHost::IsAvailable() const {
  switch (phase_) {
    case Phase::kInitial:
    case Phase::kStarted:
      return true;
    case Phase::kClosed:
    case Phase::kTerminationSent:
    case Phase::kTerminationSentAndClosed:
      return false;
  }
  NOTREACHED();
  return false;
}

base::WeakPtr<SharedWorkerHost> SharedWorkerHost::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void SharedWorkerHost::AddClient(blink::mojom::SharedWorkerClientPtr client,
                                 int client_process_id,
                                 int frame_id,
                                 const blink::MessagePortChannel& port) {
  // Pass the actual creation context type, so the client can understand if
  // there is a mismatch between security levels.
  client->OnCreated(instance_->creation_context_type());

  clients_.emplace_back(std::move(client), next_connection_request_id_++,
                        client_process_id, frame_id);
  ClientInfo& info = clients_.back();

  // Observe when the client goes away.
  info.client.set_connection_error_handler(base::BindOnce(
      &SharedWorkerHost::OnClientConnectionLost, weak_factory_.GetWeakPtr()));

  worker_->Connect(info.connection_request_id, port.ReleaseHandle());
}

void SharedWorkerHost::BindDevToolsAgent(
    blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
    blink::mojom::DevToolsAgentAssociatedRequest request) {
  worker_->BindDevToolsAgent(std::move(host), std::move(request));
}

void SharedWorkerHost::SetAppCacheHandle(
    std::unique_ptr<AppCacheNavigationHandle> appcache_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  appcache_handle_ = std::move(appcache_handle);
}

void SharedWorkerHost::SetServiceWorkerHandle(
    std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  service_worker_handle_ = std::move(service_worker_handle);
}

void SharedWorkerHost::OnClientConnectionLost() {
  // We'll get a notification for each dropped connection.
  for (auto it = clients_.begin(); it != clients_.end(); ++it) {
    if (it->client.encountered_error()) {
      clients_.erase(it);
      break;
    }
  }
  // If there are no clients left, then it's cleanup time.
  if (clients_.empty())
    TerminateWorker();
}

void SharedWorkerHost::OnWorkerConnectionLost() {
  // This will destroy |this| resulting in client's observing their mojo
  // connection being dropped.
  service_->DestroyHost(this);
}

void SharedWorkerHost::GetInterface(
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle interface_pipe) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  if (!worker_process_host)
    return;

  BindWorkerInterface(interface_name, std::move(interface_pipe),
                      worker_process_host,
                      url::Origin::Create(instance()->url()));
}

}  // namespace content
