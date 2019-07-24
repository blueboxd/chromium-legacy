// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_HOST_H_
#define CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_HOST_H_

#include <list>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/service_manager/public/mojom/interface_provider.mojom.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_client.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_host.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

class GURL;

namespace blink {
class MessagePortChannel;
class URLLoaderFactoryBundleInfo;
}

namespace content {

class AppCacheNavigationHandle;
class ServiceWorkerNavigationHandle;
class ServiceWorkerObjectHost;
class SharedWorkerContentSettingsProxyImpl;
class SharedWorkerInstance;
class SharedWorkerServiceImpl;

// The SharedWorkerHost is the interface that represents the browser side of
// the browser <-> worker communication channel. This is owned by
// SharedWorkerServiceImpl and destructed when a worker context or worker's
// message filter is closed.
class CONTENT_EXPORT SharedWorkerHost
    : public blink::mojom::SharedWorkerHost,
      public service_manager::mojom::InterfaceProvider {
 public:
  SharedWorkerHost(SharedWorkerServiceImpl* service,
                   std::unique_ptr<SharedWorkerInstance> instance,
                   int worker_process_id);
  ~SharedWorkerHost() override;

  // Allows overriding the URLLoaderFactory creation for subresources.
  // Passing a null callback will restore the default behavior.
  // This method must be called either on the UI thread or before threads start.
  // This callback is run on the UI thread.
  using CreateNetworkFactoryCallback = base::RepeatingCallback<void(
      network::mojom::URLLoaderFactoryRequest request,
      int worker_process_id,
      network::mojom::URLLoaderFactoryPtrInfo original_factory)>;
  static void SetNetworkFactoryForTesting(
      const CreateNetworkFactoryCallback& url_loader_factory_callback);

  // Starts the SharedWorker in the renderer process.
  //
  // |main_script_load_params| is sent to the renderer process and to be used to
  // load the shared worker main script pre-requested by the browser process.
  //
  // |subresource_loader_factories| is sent to the renderer process and is to be
  // used to request subresources where applicable. For example, this allows the
  // shared worker to load chrome-extension:// URLs which the renderer's default
  // loader factory can't load.
  //
  // |controller| contains information about the service worker controller. Once
  // a ServiceWorker object about the controller is prepared, it is registered
  // to |controller_service_worker_object_host|.
  void Start(
      blink::mojom::SharedWorkerFactoryPtr factory,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      blink::mojom::ControllerServiceWorkerInfoPtr controller,
      base::WeakPtr<ServiceWorkerObjectHost>
          controller_service_worker_object_host);

  void AllowFileSystem(const GURL& url,
                       base::OnceCallback<void(bool)> callback);
  void AllowIndexedDB(const GURL& url, base::OnceCallback<void(bool)> callback);
  void AllowCacheStorage(const GURL& url,
                         base::OnceCallback<void(bool)> callback);

  // Terminates the given worker, i.e. based on a UI action.
  void TerminateWorker();

  void AddClient(blink::mojom::SharedWorkerClientPtr client,
                 int client_process_id,
                 int frame_id,
                 const blink::MessagePortChannel& port);

  void BindDevToolsAgent(blink::mojom::DevToolsAgentHostAssociatedPtrInfo host,
                         blink::mojom::DevToolsAgentAssociatedRequest request);

  void SetAppCacheHandle(
      std::unique_ptr<AppCacheNavigationHandle> appcache_handle);
  void SetServiceWorkerHandle(
      std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle);

  SharedWorkerInstance* instance() { return instance_.get(); }
  int worker_process_id() const { return worker_process_id_; }
  bool IsAvailable() const;

  base::WeakPtr<SharedWorkerHost> AsWeakPtr();

 private:
  friend class SharedWorkerHostTest;

  enum class Phase {
    kInitial,
    kStarted,
    kClosed,
    kTerminationSent,
    kTerminationSentAndClosed
  };

  class ScopedDevToolsHandle;

  // Contains information about a client connecting to this shared worker.
  struct ClientInfo {
    ClientInfo(blink::mojom::SharedWorkerClientPtr client,
               int connection_request_id,
               int client_process_id,
               int frame_id);
    ~ClientInfo();
    blink::mojom::SharedWorkerClientPtr client;
    const int connection_request_id;
    const int client_process_id;
    const int frame_id;
  };

  using ClientList = std::list<ClientInfo>;

  // blink::mojom::SharedWorkerHost methods:
  void OnConnected(int connection_request_id) override;
  void OnContextClosed() override;
  void OnReadyForInspection() override;
  void OnScriptLoadFailed() override;
  void OnFeatureUsed(blink::mojom::WebFeature feature) override;

  // Returns the frame ids of this worker's clients.
  std::vector<GlobalFrameRoutingId> GetRenderFrameIDsForWorker();

  void AllowFileSystemResponse(base::OnceCallback<void(bool)> callback,
                               bool allowed);
  void OnClientConnectionLost();
  void OnWorkerConnectionLost();

  // service_manager::mojom::InterfaceProvider:
  void GetInterface(const std::string& interface_name,
                    mojo::ScopedMessagePipeHandle interface_pipe) override;

  void CreateNetworkFactory(network::mojom::URLLoaderFactoryRequest request);

  void AdvanceTo(Phase phase);

  mojo::Binding<blink::mojom::SharedWorkerHost> binding_;

  // |service_| owns |this|.
  SharedWorkerServiceImpl* service_;
  std::unique_ptr<SharedWorkerInstance> instance_;
  ClientList clients_;

  blink::mojom::SharedWorkerRequest worker_request_;
  blink::mojom::SharedWorkerPtr worker_;

  const int worker_process_id_;
  int next_connection_request_id_;
  std::unique_ptr<ScopedDevToolsHandle> devtools_handle_;

  // This is the set of features that this worker has used.
  std::set<blink::mojom::WebFeature> used_features_;

  std::unique_ptr<SharedWorkerContentSettingsProxyImpl> content_settings_;

  // This is kept alive during the lifetime of the shared worker, since it's
  // associated with Mojo interfaces (ServiceWorkerContainer and
  // URLLoaderFactory) that are needed to stay alive while the worker is
  // starting or running.
  blink::mojom::SharedWorkerFactoryPtr factory_;

  mojo::Binding<service_manager::mojom::InterfaceProvider>
      interface_provider_binding_;

  // The handle owns the precreated AppCacheHost until it's claimed by the
  // renderer after main script loading finishes.
  std::unique_ptr<AppCacheNavigationHandle> appcache_handle_;

  std::unique_ptr<ServiceWorkerNavigationHandle> service_worker_handle_;

  Phase phase_ = Phase::kInitial;

  base::WeakPtrFactory<SharedWorkerHost> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_SHARED_WORKER_HOST_H_
