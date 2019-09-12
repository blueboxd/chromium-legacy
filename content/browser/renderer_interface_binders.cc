// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_interface_binders.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/content_index/content_index_service_impl.h"
#include "content/browser/cookie_store/cookie_store_context.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/payments/payment_manager.h"
#include "content/browser/permissions/permission_service_context.h"
#include "content/browser/quota_dispatcher_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/websockets/websocket_connector_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "media/mojo/mojom/video_decode_perf_history.mojom.h"
#include "media/mojo/services/video_decode_perf_history.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/vibration_manager.mojom.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/shape_detection/public/mojom/barcodedetection_provider.mojom.h"
#include "services/shape_detection/public/mojom/facedetection_provider.mojom.h"
#include "services/shape_detection/public/mojom/shape_detection_service.mojom.h"
#include "services/shape_detection/public/mojom/textdetection.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/cache_storage/cache_storage.mojom.h"
#include "third_party/blink/public/mojom/cookie_store/cookie_store.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_manager.mojom.h"
#include "third_party/blink/public/mojom/notifications/notification_service.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

// A holder for a parameterized BinderRegistry for content-layer interfaces
// exposed to web workers.
class RendererInterfaceBinders {
 public:
  RendererInterfaceBinders() { InitializeParameterizedBinderRegistry(); }

  // Bind an interface request |interface_pipe| for |interface_name| received
  // from a web worker with origin |origin| hosted in the renderer |host|.
  void BindInterface(const std::string& interface_name,
                     mojo::ScopedMessagePipeHandle interface_pipe,
                     RenderProcessHost* host,
                     const url::Origin& origin) {
    if (parameterized_binder_registry_.TryBindInterface(
            interface_name, &interface_pipe, host, origin)) {
      return;
    }

    GetContentClient()->browser()->BindInterfaceRequestFromWorker(
        host, origin, interface_name, std::move(interface_pipe));
  }

  // Try binding an interface request |interface_pipe| for |interface_name|
  // received from |frame|.
  bool TryBindInterface(const std::string& interface_name,
                        mojo::ScopedMessagePipeHandle* interface_pipe,
                        RenderFrameHost* frame) {
    return parameterized_binder_registry_.TryBindInterface(
        interface_name, interface_pipe, frame->GetProcess(),
        frame->GetLastCommittedOrigin());
  }

 private:
  void InitializeParameterizedBinderRegistry();

  static void CreateWebSocketConnector(
      mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver,
      RenderProcessHost* host,
      const url::Origin& origin);

  service_manager::BinderRegistryWithArgs<RenderProcessHost*,
                                          const url::Origin&>
      parameterized_binder_registry_;
};

void BindShapeDetectionServiceOnIOThread(
    mojo::PendingReceiver<shape_detection::mojom::ShapeDetectionService>
        receiver) {
  auto* gpu = GpuProcessHost::Get();
  if (gpu)
    gpu->RunService(std::move(receiver));
}

shape_detection::mojom::ShapeDetectionService* GetShapeDetectionService() {
  static base::NoDestructor<
      mojo::Remote<shape_detection::mojom::ShapeDetectionService>>
      remote;
  if (!*remote) {
    base::PostTask(FROM_HERE, {BrowserThread::IO},
                   base::BindOnce(&BindShapeDetectionServiceOnIOThread,
                                  remote->BindNewPipeAndPassReceiver()));
    remote->reset_on_disconnect();
  }

  return remote->get();
}

void BindBarcodeDetectionProvider(
    shape_detection::mojom::BarcodeDetectionProviderRequest request,
    RenderProcessHost* host,
    const url::Origin& origin) {
  GetShapeDetectionService()->BindBarcodeDetectionProvider(std::move(request));
}

void BindFaceDetectionProvider(
    shape_detection::mojom::FaceDetectionProviderRequest request,
    RenderProcessHost* host,
    const url::Origin& origin) {
  GetShapeDetectionService()->BindFaceDetectionProvider(std::move(request));
}

void BindTextDetection(shape_detection::mojom::TextDetectionRequest request,
                       RenderProcessHost* host,
                       const url::Origin& origin) {
  GetShapeDetectionService()->BindTextDetection(std::move(request));
}

// Forwards service requests to Service Manager since the renderer cannot launch
// out-of-process services on is own.
template <typename Interface>
void ForwardServiceRequest(const char* service_name,
                           mojo::InterfaceRequest<Interface> request,
                           RenderProcessHost* host,
                           const url::Origin& origin) {
  auto* connector = BrowserContext::GetConnectorFor(host->GetBrowserContext());
  connector->BindInterface(service_name, std::move(request));
}

// Register renderer-exposed interfaces. Each registered interface binder is
// exposed to all renderer-hosted execution context types (document/frame,
// dedicated worker, shared worker and service worker) where the appropriate
// capability spec in the content_browser manifest includes the interface. For
// interface requests from frames, binders registered on the frame itself
// override binders registered here.
void RendererInterfaceBinders::InitializeParameterizedBinderRegistry() {
  parameterized_binder_registry_.AddInterface(
      base::BindRepeating(&BindBarcodeDetectionProvider));
  parameterized_binder_registry_.AddInterface(
      base::BindRepeating(&BindFaceDetectionProvider));
  parameterized_binder_registry_.AddInterface(
      base::BindRepeating(&BindTextDetection));

  parameterized_binder_registry_.AddInterface(
      base::Bind(&ForwardServiceRequest<device::mojom::VibrationManager>,
                 device::mojom::kServiceName));

  // Used for shared workers and service workers to create a websocket.
  // In other cases, RenderFrameHostImpl for documents or DedicatedWorkerHost
  // for dedicated workers handles interface requests in order to associate
  // websockets with a frame. Shared workers and service workers don't have to
  // do it because they don't have a frame.
  // TODO(nhiroki): Consider moving this into SharedWorkerHost and
  // ServiceWorkerProviderHost.
  parameterized_binder_registry_.AddInterface(
      base::BindRepeating(CreateWebSocketConnector));

  parameterized_binder_registry_.AddInterface(base::Bind(
      [](mojo::PendingReceiver<payments::mojom::PaymentManager> receiver,
         RenderProcessHost* host, const url::Origin& origin) {
        static_cast<StoragePartitionImpl*>(host->GetStoragePartition())
            ->GetPaymentAppContext()
            ->CreatePaymentManager(std::move(receiver));
      }));
  parameterized_binder_registry_.AddInterface(base::BindRepeating(
      [](mojo::PendingReceiver<blink::mojom::CacheStorage> receiver,
         RenderProcessHost* host, const url::Origin& origin) {
        static_cast<RenderProcessHostImpl*>(host)->BindCacheStorage(
            std::move(receiver), origin);
      }));
  parameterized_binder_registry_.AddInterface(base::BindRepeating(
      [](mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
         RenderProcessHost* host, const url::Origin& origin) {
        static_cast<RenderProcessHostImpl*>(host)->BindIndexedDB(
            std::move(receiver), origin);
      }));
  if (base::FeatureList::IsEnabled(blink::features::kNativeFileSystemAPI)) {
    parameterized_binder_registry_.AddInterface(base::BindRepeating(
        [](mojo::PendingReceiver<blink::mojom::NativeFileSystemManager>
               receiver,
           RenderProcessHost* host, const url::Origin& origin) {
          // This code path is only for workers, hence always pass in
          // MSG_ROUTING_NONE as frame ID. Frames themselves go through
          // RenderFrameHostImpl instead.
          NativeFileSystemManagerImpl::BindReceiverFromUIThread(
              static_cast<StoragePartitionImpl*>(host->GetStoragePartition()),
              NativeFileSystemManagerImpl::BindingContext(
                  origin,
                  // TODO(https://crbug.com/989323): Obtain and use a better URL
                  // for workers instead of the origin as source url. This URL
                  // will be used for SafeBrowsing checks and for the Quarantine
                  // Service.
                  origin.GetURL(), host->GetID(), MSG_ROUTING_NONE),
              std::move(receiver));
        }));
  }
  parameterized_binder_registry_.AddInterface(base::Bind(
      [](mojo::PendingReceiver<blink::mojom::PermissionService> receiver,
         RenderProcessHost* host, const url::Origin& origin) {
        static_cast<RenderProcessHostImpl*>(host)
            ->permission_service_context()
            .CreateServiceForWorker(std::move(receiver), origin);
      }));

  parameterized_binder_registry_.AddInterface(base::BindRepeating(
      [](blink::mojom::NotificationServiceRequest request,
         RenderProcessHost* host, const url::Origin& origin) {
        static_cast<StoragePartitionImpl*>(host->GetStoragePartition())
            ->GetPlatformNotificationContext()
            ->CreateService(origin, std::move(request));
      }));
  parameterized_binder_registry_.AddInterface(
      base::BindRepeating(&QuotaDispatcherHost::CreateForWorker));
  parameterized_binder_registry_.AddInterface(base::BindRepeating(
      [](blink::mojom::CookieStoreRequest request, RenderProcessHost* host,
         const url::Origin& origin) {
        static_cast<StoragePartitionImpl*>(host->GetStoragePartition())
            ->GetCookieStoreContext()
            ->CreateService(std::move(request), origin);
      }));
  parameterized_binder_registry_.AddInterface(base::BindRepeating(
      [](media::mojom::VideoDecodePerfHistoryRequest request,
         RenderProcessHost* host, const url::Origin& origin) {
        host->GetBrowserContext()->GetVideoDecodePerfHistory()->BindRequest(
            std::move(request));
      }));
  parameterized_binder_registry_.AddInterface(
      base::BindRepeating(&ContentIndexServiceImpl::Create));
}

RendererInterfaceBinders& GetRendererInterfaceBinders() {
  static base::NoDestructor<RendererInterfaceBinders> binders;
  return *binders;
}

void RendererInterfaceBinders::CreateWebSocketConnector(
    mojo::PendingReceiver<blink::mojom::WebSocketConnector> receiver,
    RenderProcessHost* host,
    const url::Origin& origin) {
  // TODO(jam): is it ok to not send extraHeaders for sockets created from
  // shared and service workers?
  mojo::MakeSelfOwnedReceiver(std::make_unique<WebSocketConnectorImpl>(
                                  host->GetID(), MSG_ROUTING_NONE, origin),
                              std::move(receiver));
}

}  // namespace

void BindWorkerInterface(const std::string& interface_name,
                         mojo::ScopedMessagePipeHandle interface_pipe,
                         RenderProcessHost* host,
                         const url::Origin& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GetRendererInterfaceBinders().BindInterface(
      interface_name, std::move(interface_pipe), host, origin);
}

bool TryBindFrameInterface(const std::string& interface_name,
                           mojo::ScopedMessagePipeHandle* interface_pipe,
                           RenderFrameHost* frame) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return GetRendererInterfaceBinders().TryBindInterface(interface_name,
                                                        interface_pipe, frame);
}

}  // namespace content
