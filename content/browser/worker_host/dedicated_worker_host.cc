// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "content/browser/worker_host/dedicated_worker_host.h"

#include "base/bind.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/websockets/websocket_manager.h"
#include "content/browser/worker_host/worker_script_fetch_initiator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "net/base/network_isolation_key.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

// A host for a single dedicated worker. Its lifetime is managed by the
// DedicatedWorkerGlobalScope of the corresponding worker in the renderer via a
// StrongBinding. This lives on the UI thread.
class DedicatedWorkerHost : public service_manager::mojom::InterfaceProvider {
 public:
  DedicatedWorkerHost(int process_id,
                      int ancestor_render_frame_id,
                      const url::Origin& origin)
      : process_id_(process_id),
        ancestor_render_frame_id_(ancestor_render_frame_id),
        origin_(origin),
        weak_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RegisterMojoInterfaces();
  }

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
    if (!process)
      return;

    // See if the registry that is specific to this worker host wants to handle
    // the interface request.
    if (registry_.TryBindInterface(interface_name, &interface_pipe))
      return;

    BindWorkerInterface(interface_name, std::move(interface_pipe), process,
                        origin_);
  }

  // PlzDedicatedWorker:
  void StartScriptLoad(
      const GURL& script_url,
      const url::Origin& request_initiator_origin,
      network::mojom::FetchCredentialsMode credentials_mode,
      blink::mojom::BlobURLTokenPtr blob_url_token,
      blink::mojom::DedicatedWorkerHostFactoryClientPtr client) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(blink::features::IsPlzDedicatedWorkerEnabled());

    auto* render_process_host = RenderProcessHost::FromID(process_id_);
    if (!render_process_host) {
      client->OnScriptLoadStartFailed();
      return;
    }
    auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
        render_process_host->GetStoragePartition());

    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory;
    if (script_url.SchemeIsBlob()) {
      if (!blob_url_token) {
        mojo::ReportBadMessage("DWH_NO_BLOB_URL_TOKEN");
        return;
      }
      blob_url_loader_factory =
          ChromeBlobStorageContext::URLLoaderFactoryForToken(
              storage_partition_impl->browser_context(),
              std::move(blob_url_token));
    } else if (blob_url_token) {
      mojo::ReportBadMessage("DWH_NOT_BLOB_URL");
      return;
    }

    appcache_handle_ = std::make_unique<AppCacheNavigationHandle>(
        storage_partition_impl->GetAppCacheService(), process_id_);

    WorkerScriptFetchInitiator::Start(
        process_id_, script_url, request_initiator_origin, credentials_mode,
        ResourceType::kWorker,
        storage_partition_impl->GetServiceWorkerContext(),
        appcache_handle_->core(), std::move(blob_url_loader_factory), nullptr,
        storage_partition_impl,
        base::BindOnce(&DedicatedWorkerHost::DidStartScriptLoad,
                       weak_factory_.GetWeakPtr(), std::move(client)));
  }

 private:
  void RegisterMojoInterfaces() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    registry_.AddInterface(base::BindRepeating(
        &DedicatedWorkerHost::CreateWebSocket, base::Unretained(this)));
    registry_.AddInterface(base::BindRepeating(
        &DedicatedWorkerHost::CreateWebUsbService, base::Unretained(this)));
    registry_.AddInterface(base::BindRepeating(
        &DedicatedWorkerHost::CreateDedicatedWorker, base::Unretained(this)));
    registry_.AddInterface(base::BindRepeating(
        &DedicatedWorkerHost::CreateIdleManager, base::Unretained(this)));
  }

  // Called from WorkerScriptFetchInitiator. Continues starting the dedicated
  // worker in the renderer process.
  //
  // |service_worker_provider_info| is sent to the renderer process and contains
  // information about its ServiceWorkerProviderHost, the browser-side host for
  // supporting the dedicated worker as a service worker client.
  //
  // |main_script_loader_factory| is not used when NetworkService is enabled.
  //
  // |main_script_load_params| is sent to the renderer process and to be used to
  // load the dedicated worker main script pre-requested by the browser process.
  //
  // |subresource_loader_factories| is sent to the renderer process and is to be
  // used to request subresources where applicable. For example, this allows the
  // dedicated worker to load chrome-extension:// URLs which the renderer's
  // default loader factory can't load.
  //
  // NetworkService (PlzWorker):
  // |controller| contains information about the service worker controller. Once
  // a ServiceWorker object about the controller is prepared, it is registered
  // to |controller_service_worker_object_host|.
  void DidStartScriptLoad(
      blink::mojom::DedicatedWorkerHostFactoryClientPtr client,
      blink::mojom::ServiceWorkerProviderInfoForWorkerPtr
          service_worker_provider_info,
      network::mojom::URLLoaderFactoryPtr main_script_loader_factory,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host,
      bool success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(blink::features::IsPlzDedicatedWorkerEnabled());
    // |main_script_loader_factory| is not used when NetworkService is enabled.
    DCHECK(!main_script_loader_factory);

    if (!success) {
      client->OnScriptLoadStartFailed();
      return;
    }

    auto* render_process_host = RenderProcessHost::FromID(process_id_);
    if (!render_process_host) {
      client->OnScriptLoadStartFailed();
      return;
    }

    // Set up the default network loader factory.
    network::mojom::URLLoaderFactoryPtrInfo default_factory_info;
    CreateNetworkFactory(mojo::MakeRequest(&default_factory_info),
                         render_process_host);
    subresource_loader_factories->default_factory_info() =
        std::move(default_factory_info);

    // Prepare the controller service worker info to pass to the renderer.
    // |object_info| can be nullptr when the service worker context or the
    // service worker version is gone during dedicated worker startup.
    blink::mojom::ServiceWorkerObjectAssociatedPtrInfo
        service_worker_remote_object;
    blink::mojom::ServiceWorkerState service_worker_state;
    if (controller && controller->object_info) {
      controller->object_info->request =
          mojo::MakeRequest(&service_worker_remote_object);
      service_worker_state = controller->object_info->state;
    }

    client->OnScriptLoadStarted(std::move(service_worker_provider_info),
                                std::move(main_script_load_params),
                                std::move(subresource_loader_factories),
                                std::move(controller));

    // |service_worker_remote_object| is an associated interface ptr, so calls
    // can't be made on it until its request endpoint is sent. Now that the
    // request endpoint was sent, it can be used, so add it to
    // ServiceWorkerObjectHost.
    if (service_worker_remote_object) {
      base::PostTaskWithTraits(
          FROM_HERE, {BrowserThread::IO},
          base::BindOnce(
              &ServiceWorkerObjectHost::AddRemoteObjectPtrAndUpdateState,
              controller_service_worker_object_host,
              std::move(service_worker_remote_object), service_worker_state));
    }
  }

  void CreateNetworkFactory(network::mojom::URLLoaderFactoryRequest request,
                            RenderProcessHost* process) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    network::mojom::TrustedURLLoaderHeaderClientPtrInfo no_header_client;

    // TODO(crbug.com/955476): Create network_isolation_key cache key using
    // worker script's origin.
    process->CreateURLLoaderFactory(
        origin_, net::NetworkIsolationKey() /* network_isolation_key */,
        std::move(no_header_client), std::move(request));
  }

  void CreateWebUsbService(blink::mojom::WebUsbServiceRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto* host =
        RenderFrameHostImpl::FromID(process_id_, ancestor_render_frame_id_);
    GetContentClient()->browser()->CreateWebUsbService(host,
                                                       std::move(request));
  }

  void CreateWebSocket(network::mojom::WebSocketRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    network::mojom::AuthenticationHandlerPtr auth_handler;
    auto* frame =
        RenderFrameHost::FromID(process_id_, ancestor_render_frame_id_);
    if (!frame) {
      // In some cases |frame| can be null. In such cases the worker will
      // soon be terminated too, so let's abort the connection.
      request.ResetWithReason(network::mojom::WebSocket::kInsufficientResources,
                              "The parent frame has already been gone.");
      return;
    }

    uint32_t options = network::mojom::kWebSocketOptionNone;
    network::mojom::TrustedHeaderClientPtr header_client;
    GetContentClient()->browser()->WillCreateWebSocket(
        frame, &request, &auth_handler, &header_client, &options);

    WebSocketManager::CreateWebSocket(
        process_id_, ancestor_render_frame_id_, origin_, options,
        std::move(auth_handler), std::move(header_client), std::move(request));
  }

  void CreateDedicatedWorker(
      blink::mojom::DedicatedWorkerHostFactoryRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CreateDedicatedWorkerHostFactory(process_id_, ancestor_render_frame_id_,
                                     origin_, std::move(request));
  }

  void CreateIdleManager(blink::mojom::IdleManagerRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto* host =
        RenderFrameHostImpl::FromID(process_id_, ancestor_render_frame_id_);
    if (!host->IsFeatureEnabled(
            blink::mojom::FeaturePolicyFeature::kIdleDetection)) {
      mojo::ReportBadMessage("Feature policy blocks access to IdleDetection.");
      return;
    }
    static_cast<StoragePartitionImpl*>(
        host->GetProcess()->GetStoragePartition())
        ->GetIdleManager()
        ->CreateService(std::move(request));
  }

  const int process_id_;
  // ancestor_render_frame_id_ is the id of the frame that owns this worker,
  // either directly, or (in the case of nested workers) indirectly via a tree
  // of dedicated workers.
  const int ancestor_render_frame_id_;
  const url::Origin origin_;

  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  service_manager::BinderRegistry registry_;

  base::WeakPtrFactory<DedicatedWorkerHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHost);
};

// A factory for creating DedicatedWorkerHosts. Its lifetime is managed by
// the renderer over mojo via a StrongBinding. This lives on the UI thread.
class DedicatedWorkerHostFactoryImpl
    : public blink::mojom::DedicatedWorkerHostFactory {
 public:
  DedicatedWorkerHostFactoryImpl(int process_id,
                                 int ancestor_render_frame_id,
                                 const url::Origin& parent_context_origin)
      : process_id_(process_id),
        ancestor_render_frame_id_(ancestor_render_frame_id),
        parent_context_origin_(parent_context_origin) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  // blink::mojom::DedicatedWorkerHostFactory:
  void CreateWorkerHost(
      const url::Origin& origin,
      service_manager::mojom::InterfaceProviderRequest request) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (blink::features::IsPlzDedicatedWorkerEnabled()) {
      mojo::ReportBadMessage("DWH_INVALID_WORKER_CREATION");
      return;
    }

    // TODO(crbug.com/729021): Once |parent_context_origin_| no longer races
    // with the request for |DedicatedWorkerHostFactory|, enforce that
    // the worker's origin either matches the origin of the creating context
    // (Document or DedicatedWorkerGlobalScope), or is unique.
    mojo::MakeStrongBinding(std::make_unique<DedicatedWorkerHost>(
                                process_id_, ancestor_render_frame_id_, origin),
                            FilterRendererExposedInterfaces(
                                blink::mojom::kNavigation_DedicatedWorkerSpec,
                                process_id_, std::move(request)));
  }

  // PlzDedicatedWorker:
  void CreateWorkerHostAndStartScriptLoad(
      const GURL& script_url,
      const url::Origin& request_initiator_origin,
      network::mojom::FetchCredentialsMode credentials_mode,
      blink::mojom::BlobURLTokenPtr blob_url_token,
      blink::mojom::DedicatedWorkerHostFactoryClientPtr client) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!blink::features::IsPlzDedicatedWorkerEnabled()) {
      mojo::ReportBadMessage("DWH_BROWSER_SCRIPT_FETCH_DISABLED");
      return;
    }

    // TODO(crbug.com/729021): Once |parent_context_origin_| no longer races
    // with the request for |DedicatedWorkerHostFactory|, enforce that
    // the worker's origin either matches the origin of the creating context
    // (Document or DedicatedWorkerGlobalScope), or is unique.
    auto host = std::make_unique<DedicatedWorkerHost>(
        process_id_, ancestor_render_frame_id_, request_initiator_origin);
    auto* host_raw = host.get();
    service_manager::mojom::InterfaceProviderPtr interface_provider;
    mojo::MakeStrongBinding(
        std::move(host),
        FilterRendererExposedInterfaces(
            blink::mojom::kNavigation_DedicatedWorkerSpec, process_id_,
            mojo::MakeRequest(&interface_provider)));
    client->OnWorkerHostCreated(std::move(interface_provider));
    host_raw->StartScriptLoad(script_url, request_initiator_origin,
                              credentials_mode, std::move(blob_url_token),
                              std::move(client));
  }

 private:
  const int process_id_;
  const int ancestor_render_frame_id_;
  const url::Origin parent_context_origin_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHostFactoryImpl);
};

}  // namespace

void CreateDedicatedWorkerHostFactory(
    int process_id,
    int ancestor_render_frame_id,
    const url::Origin& origin,
    blink::mojom::DedicatedWorkerHostFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeStrongBinding(std::make_unique<DedicatedWorkerHostFactoryImpl>(
                              process_id, ancestor_render_frame_id, origin),
                          std::move(request));
}

}  // namespace content
