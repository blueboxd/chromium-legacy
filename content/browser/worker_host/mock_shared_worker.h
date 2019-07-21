// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_MOCK_SHARED_WORKER_H_
#define CONTENT_BROWSER_WORKER_HOST_MOCK_SHARED_WORKER_H_

#include <memory>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "content/browser/worker_host/shared_worker_host.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/renderer_preferences.mojom.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker.mojom.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_provider.mojom.h"
#include "third_party/blink/public/mojom/worker/shared_worker_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_content_settings_proxy.mojom.h"

class GURL;

namespace blink {
class URLLoaderFactoryBundleInfo;
}  // namespace blink

namespace content {

class MockSharedWorker : public blink::mojom::SharedWorker {
 public:
  explicit MockSharedWorker(blink::mojom::SharedWorkerRequest request);
  ~MockSharedWorker() override;

  bool CheckReceivedConnect(int* connection_request_id,
                            blink::MessagePortChannel* port);
  bool CheckNotReceivedConnect();
  bool CheckReceivedTerminate();

 private:
  // blink::mojom::SharedWorker methods:
  void Connect(int connection_request_id,
               mojo::ScopedMessagePipeHandle port) override;
  void Terminate() override;
  void BindDevToolsAgent(
      blink::mojom::DevToolsAgentHostAssociatedPtrInfo host_ptr_info,
      blink::mojom::DevToolsAgentAssociatedRequest request) override;

  mojo::Binding<blink::mojom::SharedWorker> binding_;
  std::queue<std::pair<int, blink::MessagePortChannel>> connect_received_;
  bool terminate_received_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockSharedWorker);
};

class MockSharedWorkerFactory : public blink::mojom::SharedWorkerFactory {
 public:
  explicit MockSharedWorkerFactory(
      blink::mojom::SharedWorkerFactoryRequest request);
  ~MockSharedWorkerFactory() override;

  bool CheckReceivedCreateSharedWorker(
      const GURL& expected_url,
      const std::string& expected_name,
      blink::mojom::ContentSecurityPolicyType
          expected_content_security_policy_type,
      blink::mojom::SharedWorkerHostPtr* host,
      blink::mojom::SharedWorkerRequest* request);

 private:
  // blink::mojom::SharedWorkerFactory methods:
  void CreateSharedWorker(
      blink::mojom::SharedWorkerInfoPtr info,
      bool pause_on_start,
      const base::UnguessableToken& devtools_worker_token,
      blink::mojom::RendererPreferencesPtr renderer_preferences,
      blink::mojom::RendererPreferenceWatcherRequest preference_watcher_request,
      blink::mojom::WorkerContentSettingsProxyPtr content_settings,
      blink::mojom::ServiceWorkerProviderInfoForClientPtr
          service_worker_provider_info,
      const base::Optional<base::UnguessableToken>& appcache_host_id,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::URLLoaderFactoryBundleInfo>
          subresource_loader_factories,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info,
      blink::mojom::SharedWorkerHostPtr host,
      blink::mojom::SharedWorkerRequest request,
      service_manager::mojom::InterfaceProviderPtr interface_provider) override;

  struct CreateParams {
    CreateParams();
    ~CreateParams();
    blink::mojom::SharedWorkerInfoPtr info;
    bool pause_on_start;
    blink::mojom::WorkerContentSettingsProxyPtr content_settings;
    blink::mojom::SharedWorkerHostPtr host;
    blink::mojom::SharedWorkerRequest request;
    service_manager::mojom::InterfaceProviderPtr interface_provider;
  };

  mojo::Binding<blink::mojom::SharedWorkerFactory> binding_;
  std::unique_ptr<CreateParams> create_params_;

  DISALLOW_COPY_AND_ASSIGN(MockSharedWorkerFactory);
};

class MockSharedWorkerClient : public blink::mojom::SharedWorkerClient {
 public:
  MockSharedWorkerClient();
  ~MockSharedWorkerClient() override;

  void Bind(blink::mojom::SharedWorkerClientRequest request);
  void Close();
  bool CheckReceivedOnCreated();
  bool CheckReceivedOnConnected(
      std::set<blink::mojom::WebFeature> expected_used_features);
  bool CheckReceivedOnFeatureUsed(blink::mojom::WebFeature expected_feature);
  bool CheckNotReceivedOnFeatureUsed();
  bool CheckReceivedOnScriptLoadFailed();

 private:
  // blink::mojom::SharedWorkerClient methods:
  void OnCreated(blink::mojom::SharedWorkerCreationContextType
                     creation_context_type) override;
  void OnConnected(
      const std::vector<blink::mojom::WebFeature>& features_used) override;
  void OnScriptLoadFailed() override;
  void OnFeatureUsed(blink::mojom::WebFeature feature) override;

  mojo::Binding<blink::mojom::SharedWorkerClient> binding_;
  bool on_created_received_ = false;
  bool on_connected_received_ = false;
  std::set<blink::mojom::WebFeature> on_connected_features_;
  bool on_feature_used_received_ = false;
  blink::mojom::WebFeature on_feature_used_feature_ =
      blink::mojom::WebFeature();
  bool on_script_load_failed_ = false;

  DISALLOW_COPY_AND_ASSIGN(MockSharedWorkerClient);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_MOCK_SHARED_WORKER_H_
