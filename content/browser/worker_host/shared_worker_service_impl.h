// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_SERVICE_IMPL_H_
#define CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_SERVICE_IMPL_H_

#include <memory>
#include <set>
#include <utility>

#include "base/compiler_specific.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "content/public/browser/shared_worker_service.h"
#include "services/network/public/cpp/resource_response.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/blink/public/mojom/loader/fetch_client_settings_object.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_connector.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

namespace blink {
class MessagePortChannel;
}

namespace content {

class ChromeAppCacheService;
class SharedWorkerInstance;
class SharedWorkerHost;
class StoragePartitionImpl;

// Shared helper function
bool IsShuttingDown(RenderProcessHost* host);

// Created per StoragePartition.
class CONTENT_EXPORT SharedWorkerServiceImpl : public SharedWorkerService {
 public:
  SharedWorkerServiceImpl(
      StoragePartitionImpl* storage_partition,
      scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
      scoped_refptr<ChromeAppCacheService> appcache_service);
  ~SharedWorkerServiceImpl() override;

  // SharedWorkerService implementation.
  bool TerminateWorker(const GURL& url,
                       const std::string& name,
                       const url::Origin& constructor_origin) override;

  void TerminateAllWorkersForTesting(base::OnceClosure callback);
  void SetWorkerTerminationCallbackForTesting(base::OnceClosure callback);

  // Uses |url_loader_factory| to load workers' scripts instead of
  // StoragePartition's URLLoaderFactoryGetter.
  void SetURLLoaderFactoryForTesting(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Creates the worker if necessary or connects to an already existing worker.
  void ConnectToWorker(
      int client_process_id,
      int frame_id,
      blink::mojom::SharedWorkerInfoPtr info,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      blink::mojom::SharedWorkerClientPtr client,
      blink::mojom::SharedWorkerCreationContextType creation_context_type,
      const blink::MessagePortChannel& port,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory);

  void DestroyHost(SharedWorkerHost* host);

  StoragePartitionImpl* storage_partition() { return storage_partition_; }

 private:
  friend class SharedWorkerServiceImplTest;
  friend class SharedWorkerHostTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkServiceRestartBrowserTest, SharedWorker);

  // Creates a new worker in the client's renderer process.
  void CreateWorker(
      std::unique_ptr<SharedWorkerInstance> instance,
      blink::mojom::FetchClientSettingsObjectPtr
          outside_fetch_client_settings_object,
      blink::mojom::SharedWorkerClientPtr client,
      int client_process_id,
      int frame_id,
      const std::string& storage_domain,
      const blink::MessagePortChannel& message_port,
      scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory);
  void DidCreateScriptLoader(
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
          controller_service_worker_object_host,
      bool success);
  void StartWorker(
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
          controller_service_worker_object_host);

  // Returns nullptr if there is no such host.
  SharedWorkerHost* FindAvailableSharedWorkerHost(
      const SharedWorkerInstance& instance);

  std::set<std::unique_ptr<SharedWorkerHost>, base::UniquePtrComparator>
      worker_hosts_;
  base::OnceClosure terminate_all_workers_callback_;

  // |storage_partition_| owns |this|.
  StoragePartitionImpl* const storage_partition_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_override_;

  base::WeakPtrFactory<SharedWorkerServiceImpl> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_SERVICE_IMPL_H_
