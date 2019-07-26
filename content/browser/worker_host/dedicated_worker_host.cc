// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "content/browser/worker_host/dedicated_worker_host.h"

#include "base/bind.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/frame_host/frame_tree.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/renderer_interface_binders.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/websockets/websocket_connector_impl.h"
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
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/usb/web_usb_service.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

// A host for a single dedicated worker. Its lifetime is managed by the
// DedicatedWorkerGlobalScope of the corresponding worker in the renderer via a
// StrongBinding. This lives on the UI thread.
class DedicatedWorkerHost : public service_manager::mojom::InterfaceProvider {
 public:
  DedicatedWorkerHost(int worker_process_id,
                      int ancestor_render_frame_id,
                      const url::Origin& origin)
      : worker_process_id_(worker_process_id),
        ancestor_render_frame_id_(ancestor_render_frame_id),
        origin_(origin) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RegisterMojoInterfaces();
  }

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
    if (!worker_process_host)
      return;

    // See if the registry that is specific to this worker host wants to handle
    // the interface request.
    if (registry_.TryBindInterface(interface_name, &interface_pipe))
      return;

    BindWorkerInterface(interface_name, std::move(interface_pipe),
                        worker_process_host, origin_);
  }

  // PlzDedicatedWorker:
  void StartScriptLoad(
      const GURL& script_url,
      const url::Origin& request_initiator_origin,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      blink::mojom::BlobURLTokenPtr blob_url_token,
      blink::mojom::DedicatedWorkerHostFactoryClientPtr client) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(blink::features::IsPlzDedicatedWorkerEnabled());

    DCHECK(!client_);
    DCHECK(client);
    client_ = std::move(client);

    // Get a storage partition.
    auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
    if (!worker_process_host) {
      client_->OnScriptLoadStartFailed();
      return;
    }
    auto* storage_partition_impl = static_cast<StoragePartitionImpl*>(
        worker_process_host->GetStoragePartition());

    // Get a storage domain.
    RenderFrameHostImpl* ancestor_render_frame_host =
        GetAncestorRenderFrameHost();
    if (!ancestor_render_frame_host) {
      client_->OnScriptLoadStartFailed();
      return;
    }
    SiteInstance* site_instance = ancestor_render_frame_host->GetSiteInstance();
    if (!site_instance) {
      client_->OnScriptLoadStartFailed();
      return;
    }
    std::string storage_domain;
    std::string partition_name;
    bool in_memory;
    GetContentClient()->browser()->GetStoragePartitionConfigForSite(
        storage_partition_impl->browser_context(), site_instance->GetSiteURL(),
        /*can_be_default=*/true, &storage_domain, &partition_name, &in_memory);

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
        storage_partition_impl->GetAppCacheService(), worker_process_id_);

    service_worker_handle_ = std::make_unique<ServiceWorkerNavigationHandle>(
        storage_partition_impl->GetServiceWorkerContext());

    WorkerScriptFetchInitiator::Start(
        worker_process_id_, script_url, request_initiator_origin,
        credentials_mode, std::move(outside_fetch_client_settings_object),
        ResourceType::kWorker,
        storage_partition_impl->GetServiceWorkerContext(),
        service_worker_handle_.get(), appcache_handle_->core(),
        std::move(blob_url_loader_factory), nullptr, storage_partition_impl,
        storage_domain,
        base::BindOnce(&DedicatedWorkerHost::DidStartScriptLoad,
                       weak_factory_.GetWeakPtr()));
  }

 private:
  void RegisterMojoInterfaces() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    registry_.AddInterface(
        base::BindRepeating(&DedicatedWorkerHost::CreateWebSocketConnector,
                            base::Unretained(this)));
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
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host,
      bool success) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(blink::features::IsPlzDedicatedWorkerEnabled());

    if (!success) {
      client_->OnScriptLoadStartFailed();
      return;
    }

    // TODO(https://crbug.com/986188): Check if the main script's final response
    // URL is commitable.

    // TODO(cammie): Change this approach when we support shared workers
    // creating dedicated workers, as there might be no ancestor frame.
    RenderFrameHostImpl* ancestor_render_frame_host =
        GetAncestorRenderFrameHost();
    if (!ancestor_render_frame_host) {
      client_->OnScriptLoadStartFailed();
      return;
    }

    // Set up the default network loader factory.
    mojo::PendingRemote<network::mojom::URLLoaderFactory>
        pending_default_factory;
    CreateNetworkFactory(
        pending_default_factory.InitWithNewPipeAndPassReceiver(),
        ancestor_render_frame_host);
    subresource_loader_factories->pending_default_factory() =
        std::move(pending_default_factory);

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

    client_->OnScriptLoadStarted(service_worker_handle_->TakeProviderInfo(),
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
                            RenderFrameHostImpl* render_frame_host) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK(render_frame_host);
    network::mojom::TrustedURLLoaderHeaderClientPtrInfo no_header_client;

    // Get the origin of the frame tree's root to use as top-frame origin.
    // TODO(crbug.com/986167): Resolve issue of potential race condition.
    url::Origin top_frame_origin(render_frame_host->frame_tree_node()
                                     ->frame_tree()
                                     ->root()
                                     ->current_origin());

    RenderProcessHost* worker_process_host = render_frame_host->GetProcess();
    DCHECK(worker_process_host);
    worker_process_host->CreateURLLoaderFactory(
        origin_, nullptr /* preferences */,
        net::NetworkIsolationKey(top_frame_origin, origin_),
        std::move(no_header_client), std::move(request));
  }

  void CreateWebUsbService(blink::mojom::WebUsbServiceRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RenderFrameHostImpl* ancestor_render_frame_host =
        GetAncestorRenderFrameHost();
    // TODO(nhiroki): Check if |ancestor_render_frame_host| is valid.
    GetContentClient()->browser()->CreateWebUsbService(
        ancestor_render_frame_host, std::move(request));
  }

  void CreateWebSocketConnector(
      blink::mojom::WebSocketConnectorRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    RenderFrameHostImpl* ancestor_render_frame_host =
        GetAncestorRenderFrameHost();
    if (!ancestor_render_frame_host) {
      // In some cases |ancestor_render_frame_host| can be null. In such cases
      // the worker will soon be terminated too, so let's abort the connection.
      request.ResetWithReason(network::mojom::WebSocket::kInsufficientResources,
                              "The parent frame has already been gone.");
      return;
    }
    mojo::MakeStrongBinding(
        std::make_unique<WebSocketConnectorImpl>(
            worker_process_id_, ancestor_render_frame_id_, origin_),
        std::move(request));
  }

  void CreateDedicatedWorker(
      blink::mojom::DedicatedWorkerHostFactoryRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    CreateDedicatedWorkerHostFactory(worker_process_id_,
                                     ancestor_render_frame_id_, origin_,
                                     std::move(request));
  }

  void CreateIdleManager(blink::mojom::IdleManagerRequest request) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    RenderFrameHostImpl* ancestor_render_frame_host =
        GetAncestorRenderFrameHost();
    // TODO(nhiroki): Check if |ancestor_render_frame_host| is valid.
    if (!ancestor_render_frame_host->IsFeatureEnabled(
            blink::mojom::FeaturePolicyFeature::kIdleDetection)) {
      mojo::ReportBadMessage("Feature policy blocks access to IdleDetection.");
      return;
    }
    static_cast<StoragePartitionImpl*>(
        ancestor_render_frame_host->GetProcess()->GetStoragePartition())
        ->GetIdleManager()
        ->CreateService(std::move(request));
  }

  // May return a nullptr.
  RenderFrameHostImpl* GetAncestorRenderFrameHost() {
    // Use |worker_process_id_| as the ancestor render frame's process ID as the
    // frame surely lives in the same process for dedicated workers.
    const int ancestor_render_frame_process_id = worker_process_id_;
    return RenderFrameHostImpl::FromID(ancestor_render_frame_process_id,
                                       ancestor_render_frame_id_);
  }

  // The ID of the render process host that hosts this worker.
  const int worker_process_id_;

  // The ID of the frame that owns this worker, either directly, or (in the case
  // of nested workers) indirectly via a tree of dedicated workers.
  const int ancestor_render_frame_id_;

  const url::Origin origin_;

  // This is kept alive during the lifetime of the dedicated worker, since it's
  // associated with Mojo interfaces (ServiceWorkerContainer and
  // URLLoaderFactory) that are needed to stay alive while the worker is
  // starting or running.
  blink::mojom::DedicatedWorkerHostFactoryClientPtr client_;

  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;
  std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle_;

  service_manager::BinderRegistry registry_;

  base::WeakPtrFactory<DedicatedWorkerHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHost);
};

// A factory for creating DedicatedWorkerHosts. Its lifetime is managed by
// the renderer over mojo via a StrongBinding. This lives on the UI thread.
class DedicatedWorkerHostFactoryImpl
    : public blink::mojom::DedicatedWorkerHostFactory {
 public:
  DedicatedWorkerHostFactoryImpl(int creator_process_id,
                                 int ancestor_render_frame_id,
                                 const url::Origin& parent_context_origin)
      : creator_process_id_(creator_process_id),
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
    mojo::MakeStrongBinding(
        std::make_unique<DedicatedWorkerHost>(
            creator_process_id_, ancestor_render_frame_id_, origin),
        FilterRendererExposedInterfaces(
            blink::mojom::kNavigation_DedicatedWorkerSpec, creator_process_id_,
            std::move(request)));
  }

  // PlzDedicatedWorker:
  void CreateWorkerHostAndStartScriptLoad(
      const GURL& script_url,
      const url::Origin& request_initiator_origin,
      network::mojom::CredentialsMode credentials_mode,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      blink::mojom::BlobURLTokenPtr blob_url_token,
      blink::mojom::DedicatedWorkerHostFactoryClientPtr client) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!blink::features::IsPlzDedicatedWorkerEnabled()) {
      mojo::ReportBadMessage("DWH_BROWSER_SCRIPT_FETCH_DISABLED");
      return;
    }

    // Create a worker host that will start a new dedicated worker in the
    // renderer process whose ID is |creator_process_id_|.
    //
    // TODO(crbug.com/729021): Once |parent_context_origin_| no longer races
    // with the request for |DedicatedWorkerHostFactory|, enforce that
    // the worker's origin either matches the origin of the creating context
    // (Document or DedicatedWorkerGlobalScope), or is unique.
    auto host = std::make_unique<DedicatedWorkerHost>(creator_process_id_,
                                                      ancestor_render_frame_id_,
                                                      request_initiator_origin);
    auto* host_raw = host.get();
    service_manager::mojom::InterfaceProviderPtr interface_provider;
    mojo::MakeStrongBinding(
        std::move(host),
        FilterRendererExposedInterfaces(
            blink::mojom::kNavigation_DedicatedWorkerSpec, creator_process_id_,
            mojo::MakeRequest(&interface_provider)));
    client->OnWorkerHostCreated(std::move(interface_provider));
    host_raw->StartScriptLoad(script_url, request_initiator_origin,
                              credentials_mode,
                              std::move(outside_fetch_client_settings_object),
                              std::move(blob_url_token), std::move(client));
  }

 private:
  const int creator_process_id_;
  const int ancestor_render_frame_id_;
  const url::Origin parent_context_origin_;

  DISALLOW_COPY_AND_ASSIGN(DedicatedWorkerHostFactoryImpl);
};

}  // namespace

void CreateDedicatedWorkerHostFactory(
    int creator_process_id,
    int ancestor_render_frame_id,
    const url::Origin& origin,
    blink::mojom::DedicatedWorkerHostFactoryRequest request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  mojo::MakeStrongBinding(
      std::make_unique<DedicatedWorkerHostFactoryImpl>(
          creator_process_id, ancestor_render_frame_id, origin),
      std::move(request));
}

}  // namespace content
