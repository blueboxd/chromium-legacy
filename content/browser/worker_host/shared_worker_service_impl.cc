// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_service_impl.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <string>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/task/post_task.h"
#include "content/browser/appcache/appcache_navigation_handle.h"
#include "content/browser/appcache/appcache_navigation_handle_core.h"
#include "content/browser/file_url_loader_factory.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/shared_worker_instance.h"
#include "content/browser/worker_host/worker_script_fetch_initiator.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/bind_interface_helpers.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/loader_util.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_client.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"
#include "url/origin.h"

namespace content {

bool IsShuttingDown(RenderProcessHost* host) {
  return !host || host->FastShutdownStarted() ||
         host->IsKeepAliveRefCountDisabled();
}

SharedWorkerServiceImpl::SharedWorkerServiceImpl(
    StoragePartitionImpl* storage_partition,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    scoped_refptr<ChromeAppCacheService> appcache_service)
    : storage_partition_(storage_partition),
      service_worker_context_(std::move(service_worker_context)),
      appcache_service_(std::move(appcache_service)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

SharedWorkerServiceImpl::~SharedWorkerServiceImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

bool SharedWorkerServiceImpl::TerminateWorker(
    const GURL& url,
    const std::string& name,
    const url::Origin& constructor_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  for (auto& host : worker_hosts_) {
    if (host->IsAvailable() &&
        host->instance()->Matches(url, name, constructor_origin)) {
      host->TerminateWorker();
      return true;
    }
  }
  return false;
}

void SharedWorkerServiceImpl::TerminateAllWorkersForTesting(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!terminate_all_workers_callback_);
  if (worker_hosts_.empty()) {
    // Run callback asynchronously to avoid re-entering the caller.
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(callback));
  } else {
    terminate_all_workers_callback_ = std::move(callback);
    // Use an explicit iterator and be careful because TerminateWorker() can
    // call DestroyHost(), which removes the host from |worker_hosts_| and could
    // invalidate the iterator.
    for (auto it = worker_hosts_.begin(); it != worker_hosts_.end();)
      (*it++)->TerminateWorker();
  }
}

void SharedWorkerServiceImpl::SetWorkerTerminationCallbackForTesting(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  terminate_all_workers_callback_ = std::move(callback);
}

void SharedWorkerServiceImpl::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_override_ = std::move(url_loader_factory);
}

void SharedWorkerServiceImpl::ConnectToWorker(
    int client_process_id,
    int frame_id,
    blink::mojom::SharedWorkerInfoPtr info,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    blink::mojom::SharedWorkerClientPtr client,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    const blink::MessagePortChannel& message_port,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(client_process_id, frame_id);
  if (!render_frame_host) {
    // TODO(crbug.com/31666): Support the case where the requester is a worker
    // (i.e., nested worker).
    client->OnScriptLoadFailed();
    return;
  }

  RenderFrameHost* main_frame =
      render_frame_host->frame_tree_node()->frame_tree()->GetMainFrame();
  if (!GetContentClient()->browser()->AllowSharedWorker(
          info->url, main_frame->GetLastCommittedURL(), info->name,
          render_frame_host->GetLastCommittedOrigin(),
          WebContentsImpl::FromRenderFrameHostID(client_process_id, frame_id)
              ->GetBrowserContext(),
          client_process_id, frame_id)) {
    client->OnScriptLoadFailed();
    return;
  }

  auto instance = std::make_unique<SharedWorkerInstance>(
      info->url, info->name, render_frame_host->GetLastCommittedOrigin(),
      info->content_security_policy, info->content_security_policy_type,
      info->creation_address_space, creation_context_type);

  SharedWorkerHost* host = FindAvailableSharedWorkerHost(*instance);
  if (host) {
    // Non-secure contexts cannot connect to secure workers, and secure contexts
    // cannot connect to non-secure workers:
    if (host->instance()->creation_context_type() != creation_context_type) {
      client->OnScriptLoadFailed();
      return;
    }

    // The process may be shutting down, in which case we will try to create a
    // new shared worker instead.
    if (!IsShuttingDown(RenderProcessHost::FromID(host->worker_process_id()))) {
      host->AddClient(std::move(client), client_process_id, frame_id,
                      message_port);
      return;
    }
    // Cleanup the existing shared worker now, to avoid having two matching
    // instances. This host would likely be observing the destruction of the
    // child process shortly, but we can clean this up now to avoid some
    // complexity.
    DestroyHost(host);
  }

  // Get a storage domain.
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance) {
    client->OnScriptLoadFailed();
    return;
  }
  std::string storage_domain;
  std::string partition_name;
  bool in_memory;
  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      storage_partition_->browser_context(), site_instance->GetSiteURL(),
      /*can_be_default=*/true, &storage_domain, &partition_name, &in_memory);

  CreateWorker(std::move(instance),
               std::move(outside_fetch_client_settings_object),
               std::move(client), client_process_id, frame_id, storage_domain,
               message_port, std::move(blob_url_loader_factory));
}

void SharedWorkerServiceImpl::DestroyHost(SharedWorkerHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  worker_hosts_.erase(worker_hosts_.find(host));

  // Run the termination callback if no more workers.
  if (worker_hosts_.empty() && terminate_all_workers_callback_)
    std::move(terminate_all_workers_callback_).Run();
}

void SharedWorkerServiceImpl::CreateWorker(
    std::unique_ptr<SharedWorkerInstance> instance,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    blink::mojom::SharedWorkerClientPtr client,
    int client_process_id,
    int frame_id,
    const std::string& storage_domain,
    const blink::MessagePortChannel& message_port,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(base::FeatureList::IsEnabled(network::features::kNetworkService));
  DCHECK(!IsShuttingDown(RenderProcessHost::FromID(client_process_id)));
  DCHECK(!blob_url_loader_factory || instance->url().SchemeIsBlob());

  // Create the host. We need to do this even before starting the worker,
  // because we are about to bounce to the IO thread. If another ConnectToWorker
  // request arrives in the meantime, it finds and reuses the host instead of
  // creating a new host and therefore new SharedWorker thread.
  auto host = std::make_unique<SharedWorkerHost>(this, std::move(instance),
                                                 client_process_id);
  auto weak_host = host->AsWeakPtr();
  worker_hosts_.insert(std::move(host));

  auto appcache_handle = std::make_unique<AppCacheNavigationHandle>(
      appcache_service_.get(), weak_host->worker_process_id());
  AppCacheNavigationHandleCore* appcache_handle_core = appcache_handle_core =
      appcache_handle->core();
  weak_host->SetAppCacheHandle(std::move(appcache_handle));

  auto service_worker_handle = std::make_unique<ServiceWorkerNavigationHandle>(
      storage_partition_->GetServiceWorkerContext());
  auto* service_worker_handle_raw = service_worker_handle.get();
  weak_host->SetServiceWorkerHandle(std::move(service_worker_handle));

  // Fetch classic shared worker script with "same-origin" credentials mode.
  // https://html.spec.whatwg.org/C/#fetch-a-classic-worker-script
  //
  // TODO(crbug.com/824646, crbug.com/907749): The document should provide the
  // credentials mode specified by WorkerOptions for module script.
  const auto credentials_mode = network::mojom::CredentialsMode::kSameOrigin;

  WorkerScriptFetchInitiator::Start(
      weak_host->worker_process_id(), weak_host->instance()->url(),
      weak_host->instance()->constructor_origin(), credentials_mode,
      std::move(outside_fetch_client_settings_object),
      ResourceType::kSharedWorker, service_worker_context_,
      service_worker_handle_raw, appcache_handle_core,
      std::move(blob_url_loader_factory), url_loader_factory_override_,
      storage_partition_, storage_domain,
      base::BindOnce(&SharedWorkerServiceImpl::DidCreateScriptLoader,
                     weak_factory_.GetWeakPtr(), std::move(instance), weak_host,
                     std::move(client), client_process_id, frame_id,
                     message_port));
}

void SharedWorkerServiceImpl::DidCreateScriptLoader(
    std::unique_ptr<SharedWorkerInstance> instance,
    base::WeakPtr<SharedWorkerHost> host,
    blink::mojom::SharedWorkerClientPtr client,
    int process_id,
    int frame_id,
    const blink::MessagePortChannel& message_port,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    base::WeakPtr<ServiceWorkerObjectHost>
        controller_service_worker_object_host,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // If the script fetcher fails to load shared worker's main script, notify the
  // client of the failure and abort shared worker startup.
  if (!success) {
    client->OnScriptLoadFailed();
    return;
  }

  // TODO(https://crbug.com/986188): Check if the main script's final response
  // URL is commitable.

  StartWorker(std::move(instance), std::move(host), std::move(client),
              process_id, frame_id, message_port,
              std::move(subresource_loader_factories),
              std::move(main_script_load_params), std::move(controller),
              std::move(controller_service_worker_object_host));
}

void SharedWorkerServiceImpl::StartWorker(
    std::unique_ptr<SharedWorkerInstance> instance,
    base::WeakPtr<SharedWorkerHost> host,
    blink::mojom::SharedWorkerClientPtr client,
    int client_process_id,
    int frame_id,
    const blink::MessagePortChannel& message_port,
    std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
        subresource_loader_factories,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    base::WeakPtr<ServiceWorkerObjectHost>
        controller_service_worker_object_host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The host may already be gone if something forcibly terminated the worker
  // before it could start (e.g., in tests or a UI action). Just fail.
  if (!host)
    return;

  RenderProcessHost* worker_process_host =
      RenderProcessHost::FromID(host->worker_process_id());
  // If the target process is shutting down, then just drop this request and
  // tell the host to destruct. This also means clients that were still waiting
  // for the shared worker to start will fail.
  if (!worker_process_host || IsShuttingDown(worker_process_host)) {
    host->TerminateWorker();
    return;
  }

  // Get the factory used to instantiate the new shared worker instance in
  // the target process.
  blink::mojom::SharedWorkerFactoryPtr factory;
  BindInterface(worker_process_host, &factory);

  host->Start(std::move(factory), std::move(main_script_load_params),
              std::move(subresource_loader_factories), std::move(controller),
              std::move(controller_service_worker_object_host));
  host->AddClient(std::move(client), client_process_id, frame_id, message_port);
}

SharedWorkerHost* SharedWorkerServiceImpl::FindAvailableSharedWorkerHost(
    const SharedWorkerInstance& instance) {
  for (auto& host : worker_hosts_) {
    if (host->IsAvailable() && host->instance()->Matches(instance))
      return host.get();
  }
  return nullptr;
}

}  // namespace content
