// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_host_factory_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/browser/worker_host/dedicated_worker_service_impl.h"
#include "content/public/browser/render_process_host.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/blink/public/common/features.h"

namespace content {

DedicatedWorkerHostFactoryImpl::DedicatedWorkerHostFactoryImpl(
    int worker_process_id,
    base::Optional<GlobalFrameRoutingId> creator_render_frame_host_id,
    GlobalFrameRoutingId ancestor_render_frame_host_id,
    const url::Origin& creator_origin,
    const network::CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
        coep_reporter)
    : worker_process_id_(worker_process_id),
      creator_render_frame_host_id_(creator_render_frame_host_id),
      ancestor_render_frame_host_id_(ancestor_render_frame_host_id),
      creator_origin_(creator_origin),
      cross_origin_embedder_policy_(cross_origin_embedder_policy),
      coep_reporter_(std::move(coep_reporter)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

DedicatedWorkerHostFactoryImpl::~DedicatedWorkerHostFactoryImpl() = default;

void DedicatedWorkerHostFactoryImpl::CreateWorkerHost(
    mojo::PendingReceiver<blink::mojom::BrowserInterfaceBroker> broker_receiver,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host_receiver,
    base::OnceCallback<void(const network::CrossOriginEmbedderPolicy&)>
        callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    mojo::ReportBadMessage("DWH_INVALID_WORKER_CREATION");
    return;
  }

  std::move(callback).Run(cross_origin_embedder_policy_);

  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  if (!worker_process_host || !worker_process_host->IsInitializedAndNotDead()) {
    // Abort if the worker's process host is gone. This means that the calling
    // frame or worker is also either destroyed or in the process of being
    // destroyed.
    return;
  }

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      worker_process_host->GetStoragePartition());

  // Get the dedicated worker service.
  DedicatedWorkerServiceImpl* service =
      storage_partition->GetDedicatedWorkerService();

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());

  auto* host = new DedicatedWorkerHost(
      service, service->GenerateNextDedicatedWorkerId(), worker_process_host,
      creator_render_frame_host_id_, ancestor_render_frame_host_id_,
      creator_origin_, cross_origin_embedder_policy_, std::move(coep_reporter),
      std::move(host_receiver));
  host->BindBrowserInterfaceBrokerReceiver(std::move(broker_receiver));
}

// PlzDedicatedWorker:
void DedicatedWorkerHostFactoryImpl::CreateWorkerHostAndStartScriptLoad(
    const GURL& script_url,
    network::mojom::CredentialsMode credentials_mode,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    mojo::PendingRemote<blink::mojom::BlobURLToken> blob_url_token,
    mojo::PendingRemote<blink::mojom::DedicatedWorkerHostFactoryClient> client,
    mojo::PendingReceiver<blink::mojom::DedicatedWorkerHost> host_receiver) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
    mojo::ReportBadMessage("DWH_BROWSER_SCRIPT_FETCH_DISABLED");
    return;
  }

  // TODO(https://crbug.com/1058759): Compare |creator_origin_| to
  // |script_url|, and report as bad message if that fails.

  auto* worker_process_host = RenderProcessHost::FromID(worker_process_id_);
  if (!worker_process_host || !worker_process_host->IsInitializedAndNotDead()) {
    // Abort if the worker's process host is gone. This means that the calling
    // frame or worker is also either destroyed or in the process of being
    // destroyed.
    return;
  }

  auto* storage_partition = static_cast<StoragePartitionImpl*>(
      worker_process_host->GetStoragePartition());

  // Get the dedicated worker service.
  DedicatedWorkerServiceImpl* service =
      storage_partition->GetDedicatedWorkerService();

  mojo::PendingRemote<network::mojom::CrossOriginEmbedderPolicyReporter>
      coep_reporter;
  coep_reporter_->Clone(coep_reporter.InitWithNewPipeAndPassReceiver());

  auto* host = new DedicatedWorkerHost(
      service, service->GenerateNextDedicatedWorkerId(), worker_process_host,
      creator_render_frame_host_id_, ancestor_render_frame_host_id_,
      creator_origin_, cross_origin_embedder_policy_, std::move(coep_reporter),
      std::move(host_receiver));
  mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker> broker;
  host->BindBrowserInterfaceBrokerReceiver(
      broker.InitWithNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::DedicatedWorkerHostFactoryClient> remote_client(
      std::move(client));
  remote_client->OnWorkerHostCreated(std::move(broker));
  host->StartScriptLoad(script_url, credentials_mode,
                        std::move(outside_fetch_client_settings_object),
                        std::move(blob_url_token), std::move(remote_client));
}

}  // namespace content
