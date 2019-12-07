// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_provider_host.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/interface_provider_filtering.h"
#include "content/browser/service_worker/service_worker_consts.h"
#include "content/browser/service_worker/service_worker_container_host.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/webtransport/quic_transport_connector_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/origin_util.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/common/service_worker/service_worker_utils.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_client.mojom.h"

namespace content {

namespace {

// This function provides the next ServiceWorkerProviderHost ID.
int NextProviderId() {
  static int g_next_provider_id = 0;
  return g_next_provider_id++;
}

void CreateQuicTransportConnectorImpl(
    int process_id,
    const url::Origin& origin,
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto* process = RenderProcessHost::FromID(process_id);
  if (!process)
    return;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<QuicTransportConnectorImpl>(
          process_id, origin, net::NetworkIsolationKey(origin, origin)),
      std::move(receiver));
}

}  // anonymous namespace

// static
base::WeakPtr<ServiceWorkerProviderHost>
ServiceWorkerProviderHost::CreateForWindow(
    base::WeakPtr<ServiceWorkerContextCore> context,
    bool are_ancestors_secure,
    int frame_tree_node_id,
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote) {
  DCHECK(context);
  auto host = base::WrapUnique(new ServiceWorkerProviderHost(
      blink::mojom::ServiceWorkerProviderType::kForWindow, are_ancestors_secure,
      frame_tree_node_id, std::move(host_receiver), std::move(container_remote),
      /*running_hosted_version=*/nullptr, context));
  auto weak_ptr = host->AsWeakPtr();
  RegisterToContextCore(context, std::move(host));
  return weak_ptr;
}

// static
base::WeakPtr<ServiceWorkerProviderHost>
ServiceWorkerProviderHost::CreateForServiceWorker(
    base::WeakPtr<ServiceWorkerContextCore> context,
    scoped_refptr<ServiceWorkerVersion> version,
    blink::mojom::ServiceWorkerProviderInfoForStartWorkerPtr*
        out_provider_info) {
  auto host = base::WrapUnique(new ServiceWorkerProviderHost(
      blink::mojom::ServiceWorkerProviderType::kForServiceWorker,
      /*is_parent_frame_secure=*/true, FrameTreeNode::kFrameTreeNodeInvalidId,
      (*out_provider_info)->host_remote.InitWithNewEndpointAndPassReceiver(),
      /*container_remote=*/mojo::NullAssociatedRemote(), std::move(version),
      context));
  auto weak_ptr = host->AsWeakPtr();
  RegisterToContextCore(context, std::move(host));
  return weak_ptr;
}

// static
base::WeakPtr<ServiceWorkerProviderHost>
ServiceWorkerProviderHost::CreateForWebWorker(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int process_id,
    blink::mojom::ServiceWorkerProviderType provider_type,
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote) {
  using ServiceWorkerProviderType = blink::mojom::ServiceWorkerProviderType;
  DCHECK((base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker) &&
          provider_type == ServiceWorkerProviderType::kForDedicatedWorker) ||
         provider_type == ServiceWorkerProviderType::kForSharedWorker);
  auto host = base::WrapUnique(new ServiceWorkerProviderHost(
      provider_type, /*is_parent_frame_secure=*/true,
      FrameTreeNode::kFrameTreeNodeInvalidId, std::move(host_receiver),
      std::move(container_remote), /*running_hosted_version=*/nullptr,
      context));
  host->container_host()->SetContainerProcessId(process_id);

  auto weak_ptr = host->AsWeakPtr();
  RegisterToContextCore(context, std::move(host));
  return weak_ptr;
}

// static
void ServiceWorkerProviderHost::RegisterToContextCore(
    base::WeakPtr<ServiceWorkerContextCore> context,
    std::unique_ptr<ServiceWorkerProviderHost> host) {
  DCHECK(host->container_host()->receiver().is_bound());
  host->container_host()->receiver().set_disconnect_handler(
      base::BindOnce(&ServiceWorkerContextCore::RemoveProviderHost, context,
                     host->provider_id()));
  context->AddProviderHost(std::move(host));
}

ServiceWorkerProviderHost::ServiceWorkerProviderHost(
    blink::mojom::ServiceWorkerProviderType type,
    bool is_parent_frame_secure,
    int frame_tree_node_id,
    mojo::PendingAssociatedReceiver<blink::mojom::ServiceWorkerContainerHost>
        host_receiver,
    mojo::PendingAssociatedRemote<blink::mojom::ServiceWorkerContainer>
        container_remote,
    scoped_refptr<ServiceWorkerVersion> running_hosted_version,
    base::WeakPtr<ServiceWorkerContextCore> context)
    : provider_id_(NextProviderId()),
      running_hosted_version_(std::move(running_hosted_version)),
      container_host_(std::make_unique<content::ServiceWorkerContainerHost>(
          type,
          is_parent_frame_secure,
          frame_tree_node_id,
          std::move(host_receiver),
          std::move(container_remote),
          type == blink::mojom::ServiceWorkerProviderType::kForServiceWorker
              ? this
              : nullptr,
          context)) {
  DCHECK_NE(blink::mojom::ServiceWorkerProviderType::kUnknown, type);
  if (type == blink::mojom::ServiceWorkerProviderType::kForServiceWorker) {
    DCHECK(running_hosted_version_);
    container_host_->UpdateUrls(running_hosted_version_->script_url(),
                                running_hosted_version_->script_url(),
                                running_hosted_version_->script_origin());
  }
}

ServiceWorkerProviderHost::~ServiceWorkerProviderHost() {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  // Explicitly destroy the ServiceWorkerContainerHost to release
  // ServiceWorkerObjectHosts and ServiceWorkerRegistrationObjectHosts owned by
  // that. Otherwise, this destructor can trigger their Mojo connection error
  // handlers, which would call back into halfway destroyed |this|. This is
  // because they are associated with the ServiceWorker interface, which can be
  // destroyed while in this destructor (|running_hosted_version_|'s
  // |event_dispatcher_|). See https://crbug.com/854993.
  container_host_.reset();
}

ServiceWorkerVersion* ServiceWorkerProviderHost::running_hosted_version()
    const {
  DCHECK(!running_hosted_version_ ||
         container_host_->IsContainerForServiceWorker());
  return running_hosted_version_.get();
}

bool ServiceWorkerProviderHost::IsProviderForServiceWorker() const {
  return container_host_->IsContainerForServiceWorker();
}

void ServiceWorkerProviderHost::CompleteStartWorkerPreparation(
    int process_id,
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker>
        broker_receiver) {
  DCHECK_EQ(ChildProcessHost::kInvalidUniqueID, worker_process_id_);
  DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id);
  DCHECK(IsProviderForServiceWorker());
  SetWorkerProcessId(process_id);

  broker_receiver_.Bind(std::move(broker_receiver));
}

void ServiceWorkerProviderHost::CreateQuicTransportConnector(
    mojo::PendingReceiver<blink::mojom::QuicTransportConnector> receiver) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());
  DCHECK(IsProviderForServiceWorker());
  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&CreateQuicTransportConnectorImpl, worker_process_id_,
                     running_hosted_version_->script_origin(),
                     std::move(receiver)));
}

void ServiceWorkerProviderHost::SetWorkerProcessId(int worker_process_id) {
  DCHECK(IsProviderForServiceWorker());
  worker_process_id_ = worker_process_id;
}

}  // namespace content
