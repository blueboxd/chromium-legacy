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
#include "content/browser/loader/file_url_loader_factory.h"
#include "content/browser/service_worker/service_worker_main_resource_handle.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/browser/worker_host/worker_script_fetch_initiator.h"
#include "content/common/content_constants_internal.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/shared_worker_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/script/script_type.mojom.h"
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

  // Note: This ideally should dchecks that |worker_hosts_| is empty,
  // but some tests do not tear down everything correctly.
  worker_hosts_.clear();
}

void SharedWorkerServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SharedWorkerServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SharedWorkerServiceImpl::EnumerateSharedWorkers(Observer* observer) {
  for (const auto& host : worker_hosts_) {
    if (host->started()) {
      observer->OnWorkerStarted(host->id(), host->GetProcessHost()->GetID(),
                                host->dev_tools_token());
    }
  }
}

bool SharedWorkerServiceImpl::TerminateWorker(
    const GURL& url,
    const std::string& name,
    const url::Origin& constructor_origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SharedWorkerHost* worker_host =
      FindMatchingSharedWorkerHost(url, name, constructor_origin);
  if (worker_host) {
    DestroyHost(worker_host);
    return true;
  }

  return false;
}

void SharedWorkerServiceImpl::SetURLLoaderFactoryForTesting(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_override_ = std::move(url_loader_factory);
}

void SharedWorkerServiceImpl::ConnectToWorker(
    GlobalFrameRoutingId client_render_frame_host_id,
    blink::mojom::SharedWorkerInfoPtr info,
    mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
    blink::mojom::SharedWorkerCreationContextType creation_context_type,
    const blink::MessagePortChannel& message_port,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(client_render_frame_host_id);
  if (!render_frame_host) {
    // TODO(crbug.com/31666): Support the case where the requester is a worker
    // (i.e., nested worker).
    ScriptLoadFailed(std::move(client), /*error_message=*/"");
    return;
  }

  // Enforce same-origin policy.
  // data: URLs are not considered a different origin.
  url::Origin constructor_origin = render_frame_host->GetLastCommittedOrigin();
  bool is_cross_origin = !info->url.SchemeIs(url::kDataScheme) &&
                         url::Origin::Create(info->url) != constructor_origin;
  if (is_cross_origin &&
      !GetContentClient()->browser()->DoesSchemeAllowCrossOriginSharedWorker(
          constructor_origin.scheme())) {
    ScriptLoadFailed(std::move(client), /*error_message=*/"");
    return;
  }

  RenderFrameHost* main_frame =
      render_frame_host->frame_tree_node()->frame_tree()->GetMainFrame();
  if (!GetContentClient()->browser()->AllowSharedWorker(
          info->url,
          render_frame_host->ComputeSiteForCookies().RepresentativeUrl(),
          main_frame->GetLastCommittedOrigin(), info->options->name,
          constructor_origin,
          WebContentsImpl::FromRenderFrameHostID(client_render_frame_host_id)
              ->GetBrowserContext(),
          client_render_frame_host_id.child_id,
          client_render_frame_host_id.frame_routing_id)) {
    ScriptLoadFailed(std::move(client), /*error_message=*/"");
    return;
  }

  SharedWorkerHost* host = FindMatchingSharedWorkerHost(
      info->url, info->options->name, constructor_origin);
  if (host) {
    // Non-secure contexts cannot connect to secure workers, and secure contexts
    // cannot connect to non-secure workers:
    if (host->instance().creation_context_type() != creation_context_type) {
      ScriptLoadFailed(std::move(client), /*error_message=*/"");
      return;
    }
    // Step 11.4: "If worker global scope is not null, then check if worker
    // global scope's type and credentials match the options values. If not,
    // queue a task to fire an event named error and abort these steps."
    // https://html.spec.whatwg.org/C/#dom-sharedworker
    if (host->instance().script_type() != info->options->type ||
        host->instance().credentials_mode() != info->options->credentials) {
      ScriptLoadFailed(
          std::move(client),
          "Failed to connect an existing shared worker because the type or "
          "credentials given on the SharedWorker constructor doesn't match "
          "the existing shared worker's type or credentials.");
      return;
    }

    host->AddClient(std::move(client), client_render_frame_host_id,
                    message_port);
    return;
  }

  // Could not find an existing SharedWorkerHost to reuse. Create a new one.

  // Get a storage domain.
  SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance) {
    ScriptLoadFailed(std::move(client), /*error_message=*/"");
    return;
  }
  std::string storage_domain;
  std::string partition_name;
  bool in_memory;
  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      storage_partition_->browser_context(), site_instance->GetSiteURL(),
      /*can_be_default=*/true, &storage_domain, &partition_name, &in_memory);

  SharedWorkerInstance instance(
      info->url, info->options->type, info->options->credentials,
      info->options->name, constructor_origin, info->content_security_policy,
      info->content_security_policy_type, info->creation_address_space,
      creation_context_type);
  host = CreateWorker(shared_worker_id_generator_.GenerateNextId(), instance,
                      std::move(info->outside_fetch_client_settings_object),
                      client_render_frame_host_id, storage_domain, message_port,
                      std::move(blob_url_loader_factory));
  host->AddClient(std::move(client), client_render_frame_host_id, message_port);
}

void SharedWorkerServiceImpl::DestroyHost(SharedWorkerHost* host) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(host);

  worker_hosts_.erase(worker_hosts_.find(host));
}

void SharedWorkerServiceImpl::NotifyWorkerStarted(
    SharedWorkerId shared_worker_id,
    int worker_process_id,
    const base::UnguessableToken& dev_tools_token) {
  for (Observer& observer : observers_) {
    observer.OnWorkerStarted(shared_worker_id, worker_process_id,
                             dev_tools_token);
  }
}

void SharedWorkerServiceImpl::NotifyWorkerTerminating(
    SharedWorkerId shared_worker_id) {
  for (Observer& observer : observers_)
    observer.OnBeforeWorkerTerminated(shared_worker_id);
}

void SharedWorkerServiceImpl::NotifyClientAdded(
    SharedWorkerId shared_worker_id,
    GlobalFrameRoutingId client_render_frame_host_id) {
  auto insertion_result = shared_worker_client_counts_.insert(
      {{shared_worker_id, client_render_frame_host_id}, 0});

  int& count = insertion_result.first->second;
  ++count;

  // Only notify if this is the first time that this frame connects to that
  // shared worker.
  if (insertion_result.second) {
    for (Observer& observer : observers_)
      observer.OnClientAdded(shared_worker_id, client_render_frame_host_id);
  }
}

void SharedWorkerServiceImpl::NotifyClientRemoved(
    SharedWorkerId shared_worker_id,
    GlobalFrameRoutingId client_render_frame_host_id) {
  auto it = shared_worker_client_counts_.find(
      std::make_pair(shared_worker_id, client_render_frame_host_id));
  DCHECK(it != shared_worker_client_counts_.end());

  int& count = it->second;
  DCHECK_GT(count, 0);
  --count;

  // Only notify if there are no longer any active connections from this frame
  // to that shared worker.
  if (count == 0) {
    shared_worker_client_counts_.erase(it);
    for (Observer& observer : observers_)
      observer.OnClientRemoved(shared_worker_id, client_render_frame_host_id);
  }
}

SharedWorkerHost* SharedWorkerServiceImpl::CreateWorker(
    SharedWorkerId shared_worker_id,
    const SharedWorkerInstance& instance,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    GlobalFrameRoutingId creator_render_frame_host_id,
    const std::string& storage_domain,
    const blink::MessagePortChannel& message_port,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!blob_url_loader_factory || instance.url().SchemeIsBlob());

  // Allocate the worker in the same process as the creator.
  auto* worker_process_host =
      RenderProcessHost::FromID(creator_render_frame_host_id.child_id);
  DCHECK(!IsShuttingDown(worker_process_host));

  // Create the host. We need to do this even before starting the worker,
  // because we are about to bounce to the IO thread. If another ConnectToWorker
  // request arrives in the meantime, it finds and reuses the host instead of
  // creating a new host and therefore new SharedWorker thread.
  auto insertion_result =
      worker_hosts_.insert(std::make_unique<SharedWorkerHost>(
          this, shared_worker_id, instance, worker_process_host));
  DCHECK(insertion_result.second);
  SharedWorkerHost* host = insertion_result.first->get();

  base::WeakPtr<AppCacheHost> appcache_host;
  if (base::FeatureList::IsEnabled(blink::features::kAppCache)) {
    auto appcache_handle = std::make_unique<AppCacheNavigationHandle>(
        appcache_service_.get(), worker_process_host->GetID());
    appcache_host = appcache_handle->host()->GetWeakPtr();
    host->SetAppCacheHandle(std::move(appcache_handle));
  }

  auto service_worker_handle =
      std::make_unique<ServiceWorkerMainResourceHandle>(
          storage_partition_->GetServiceWorkerContext());
  auto* service_worker_handle_raw = service_worker_handle.get();
  host->SetServiceWorkerHandle(std::move(service_worker_handle));

  // Set the credentials mode for worker script fetch based on the script type
  // For classic worker scripts, always use "same-origin" credentials mode.
  // https://html.spec.whatwg.org/C/#fetch-a-classic-worker-script
  // For module worker script, use the credentials mode on WorkerOptions.
  // https://html.spec.whatwg.org/C/#fetch-a-module-worker-script-tree
  const auto credentials_mode =
      instance.script_type() == blink::mojom::ScriptType::kClassic
          ? network::mojom::CredentialsMode::kSameOrigin
          : instance.credentials_mode();

  RenderFrameHostImpl* creator_render_frame_host =
      RenderFrameHostImpl::FromID(creator_render_frame_host_id);
  url::Origin worker_origin = url::Origin::Create(host->instance().url());

  base::WeakPtr<SharedWorkerHost> weak_host = host->AsWeakPtr();
  // Cloning before std::move() so that the object can be used in two functions.
  auto cloned_outside_fetch_client_settings_object =
      outside_fetch_client_settings_object.Clone();
  // TODO(mmenke): The site-for-cookies and NetworkIsolationKey arguments leak
  // data across NetworkIsolationKeys and allow same-site cookies to be sent in
  // cross-site contexts. Fix this.
  WorkerScriptFetchInitiator::Start(
      worker_process_host->GetID(), host->instance().url(),
      creator_render_frame_host, net::SiteForCookies::FromOrigin(worker_origin),
      host->instance().constructor_origin(),
      net::NetworkIsolationKey(worker_origin, worker_origin), credentials_mode,
      std::move(outside_fetch_client_settings_object),
      blink::mojom::ResourceType::kSharedWorker, service_worker_context_,
      service_worker_handle_raw, std::move(appcache_host),
      std::move(blob_url_loader_factory), url_loader_factory_override_,
      storage_partition_, storage_domain,
      base::BindOnce(&SharedWorkerServiceImpl::StartWorker,
                     weak_factory_.GetWeakPtr(), weak_host, message_port,
                     std::move(cloned_outside_fetch_client_settings_object)));

  // Ensures that WorkerScriptFetchInitiator::Start() doesn't synchronously
  // destroy the SharedWorkerHost.
  DCHECK(weak_host);

  return host;
}

void SharedWorkerServiceImpl::StartWorker(
    base::WeakPtr<SharedWorkerHost> host,
    const blink::MessagePortChannel& message_port,
    blink::mojom::FetchClientSettingsObjectPtr
        outside_fetch_client_settings_object,
    bool did_fetch_worker_script,
    std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
        subresource_loader_factories,
    blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
    blink::mojom::ControllerServiceWorkerInfoPtr controller,
    base::WeakPtr<ServiceWorkerObjectHost>
        controller_service_worker_object_host,
    const GURL& final_response_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // The host may already be gone if something forcibly terminated the worker
  // before it could start (e.g., in tests, a UI action or the renderer process
  // is gone). Just fail.
  if (!host)
    return;

  // If the script fetcher failed to load the shared worker's main script,
  // terminate the worker.
  if (!did_fetch_worker_script) {
    DestroyHost(host.get());
    return;
  }

  // Also drop this request if the worker has no clients. This is possible if
  // the frame that caused this worker to be created was deleted between the
  // CreateWorker() and StartWorker() calls. Doing so avoids starting a shared
  // worker and immediately stopping it because its sole client is already being
  // torn down.
  host->PruneNonExistentClients();
  if (!host->HasClients()) {
    DestroyHost(host.get());
    return;
  }

  // TODO(https://crbug.com/986188): Check if the main script's final response
  // URL is committable.

  // Get the factory used to instantiate the new shared worker instance in
  // the target process.
  mojo::PendingRemote<blink::mojom::SharedWorkerFactory> factory;
  host->GetProcessHost()->BindReceiver(
      factory.InitWithNewPipeAndPassReceiver());

  host->Start(std::move(factory), std::move(main_script_load_params),
              std::move(subresource_loader_factories), std::move(controller),
              std::move(controller_service_worker_object_host),
              std::move(outside_fetch_client_settings_object));
  for (Observer& observer : observers_)
    observer.OnFinalResponseURLDetermined(host->id(), final_response_url);
}

SharedWorkerHost* SharedWorkerServiceImpl::FindMatchingSharedWorkerHost(
    const GURL& url,
    const std::string& name,
    const url::Origin& constructor_origin) {
  for (auto& host : worker_hosts_) {
    if (host->instance().Matches(url, name, constructor_origin))
      return host.get();
  }
  return nullptr;
}

void SharedWorkerServiceImpl::ScriptLoadFailed(
    mojo::PendingRemote<blink::mojom::SharedWorkerClient> client,
    const std::string& error_message) {
  mojo::Remote<blink::mojom::SharedWorkerClient> remote_client(
      std::move(client));
  remote_client->OnScriptLoadFailed(error_message);
}

}  // namespace content
