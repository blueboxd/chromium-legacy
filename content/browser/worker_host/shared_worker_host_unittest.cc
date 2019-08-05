// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_host.h"

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/navigation_subresource_loader_params.h"
#include "content/browser/service_worker/embedded_worker_test_helper.h"
#include "content/browser/service_worker/service_worker_navigation_handle.h"
#include "content/browser/worker_host/mock_shared_worker.h"
#include "content/browser/worker_host/shared_worker_connector_impl.h"
#include "content/browser/worker_host/shared_worker_instance.h"
#include "content/browser/worker_host/shared_worker_service_impl.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "content/test/not_implemented_network_url_loader_factory.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "url/origin.h"

using blink::MessagePortChannel;

namespace content {

class SharedWorkerHostTest : public testing::Test {
 public:
  void SetUp() override {
    helper_.reset(new EmbeddedWorkerTestHelper(base::FilePath()));
  }

  SharedWorkerHostTest()
      : default_mock_url_loader_factory_(
            std::make_unique<NotImplementedNetworkURLLoaderFactory>()),
        mock_render_process_host_(&browser_context_),
        service_(nullptr /* storage_partition */,
                 nullptr /* service_worker_context */,
                 nullptr /* appcache_service */) {
    mock_render_process_host_.OverrideURLLoaderFactory(
        default_mock_url_loader_factory_.get());
  }

  base::WeakPtr<SharedWorkerHost> CreateHost() {
    GURL url("http://www.example.com/w.js");
    std::string name("name");
    url::Origin origin = url::Origin::Create(url);
    std::string content_security_policy;
    blink::mojom::ContentSecurityPolicyType content_security_policy_type =
        blink::mojom::ContentSecurityPolicyType::kReport;
    blink::mojom::IPAddressSpace creation_address_space =
        blink::mojom::IPAddressSpace::kPublic;
    blink::mojom::SharedWorkerCreationContextType creation_context_type =
        blink::mojom::SharedWorkerCreationContextType::kSecure;

    auto instance = std::make_unique<SharedWorkerInstance>(
        url, name, origin, content_security_policy,
        content_security_policy_type, creation_address_space,
        creation_context_type);
    auto host = std::make_unique<SharedWorkerHost>(
        &service_, std::move(instance), mock_render_process_host_.GetID());
    auto weak_host = host->AsWeakPtr();
    service_.worker_hosts_.insert(std::move(host));
    return weak_host;
  }

  void StartWorker(SharedWorkerHost* host,
                   blink::mojom::SharedWorkerFactoryPtr factory) {
    auto main_script_load_params =
        blink::mojom::WorkerMainScriptLoadParams::New();
    auto subresource_loader_factories =
        std::make_unique<blink::URLLoaderFactoryBundleInfo>();

    base::Optional<SubresourceLoaderParams> subresource_loader_params =
        SubresourceLoaderParams();
    network::mojom::URLLoaderFactoryPtr loader_factory_ptr;
    mojo::MakeStrongBinding(
        std::make_unique<NotImplementedNetworkURLLoaderFactory>(),
        mojo::MakeRequest(&loader_factory_ptr));
    subresource_loader_params->pending_appcache_loader_factory =
        loader_factory_ptr.PassInterface();

    // Set up for service worker.
    auto service_worker_handle =
        std::make_unique<ServiceWorkerNavigationHandle>(
            helper_->context_wrapper());
    blink::mojom::ServiceWorkerContainerAssociatedPtrInfo client_ptr_info;
    blink::mojom::ServiceWorkerContainerHostAssociatedRequest host_request;
    auto provider_info =
        blink::mojom::ServiceWorkerProviderInfoForClient::New();
    provider_info->client_request = mojo::MakeRequest(&client_ptr_info);
    host_request = mojo::MakeRequest(&provider_info->host_ptr_info);
    base::WeakPtr<ServiceWorkerProviderHost> service_worker_host =
        ServiceWorkerProviderHost::PreCreateForWebWorker(
            helper_->context()->AsWeakPtr(), mock_render_process_host_.GetID(),
            blink::mojom::ServiceWorkerProviderType::kForSharedWorker,
            std::move(host_request), std::move(client_ptr_info));
    service_worker_handle->OnCreatedProviderHost(std::move(provider_info));
    host->SetServiceWorkerHandle(std::move(service_worker_handle));

    host->Start(std::move(factory), std::move(main_script_load_params),
                std::move(subresource_loader_factories),
                nullptr /* controller */,
                nullptr /* controller_service_worker_object_host */);
  }

  MessagePortChannel AddClient(SharedWorkerHost* host,
                               blink::mojom::SharedWorkerClientPtr client) {
    mojo::MessagePipe message_pipe;
    MessagePortChannel local_port(std::move(message_pipe.handle0));
    MessagePortChannel remote_port(std::move(message_pipe.handle1));
    host->AddClient(std::move(client), mock_render_process_host_.GetID(),
                    22 /* dummy frame_id */, std::move(remote_port));
    return local_port;
  }

 protected:
  TestBrowserThreadBundle test_browser_thread_bundle_;
  TestBrowserContext browser_context_;
  std::unique_ptr<network::mojom::URLLoaderFactory>
      default_mock_url_loader_factory_;
  MockRenderProcessHost mock_render_process_host_;
  std::unique_ptr<EmbeddedWorkerTestHelper> helper_;

  SharedWorkerServiceImpl service_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerHostTest);
};

TEST_F(SharedWorkerHostTest, Normal) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Start the worker.
  blink::mojom::SharedWorkerFactoryPtr factory;
  MockSharedWorkerFactory factory_impl(mojo::MakeRequest(&factory));
  StartWorker(host.get(), std::move(factory));

  // Add the initiating client.
  MockSharedWorkerClient client;
  blink::mojom::SharedWorkerClientPtr client_ptr;
  client.Bind(mojo::MakeRequest(&client_ptr));
  MessagePortChannel local_port = AddClient(host.get(), std::move(client_ptr));
  base::RunLoop().RunUntilIdle();

  // The factory should have gotten the CreateSharedWorker message.
  blink::mojom::SharedWorkerHostPtr worker_host;
  blink::mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory_impl.CheckReceivedCreateSharedWorker(
      host->instance()->url(), host->instance()->name(),
      host->instance()->content_security_policy_type(), &worker_host,
      &worker_request));
  {
    MockSharedWorker worker(std::move(worker_request));
    base::RunLoop().RunUntilIdle();

    // The worker and client should have gotten initial messages.
    int connection_request_id;
    MessagePortChannel port;
    EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id, &port));
    EXPECT_TRUE(client.CheckReceivedOnCreated());
    // Simulate events the shared worker would send.
    worker_host->OnReadyForInspection();
    worker_host->OnConnected(connection_request_id);
    base::RunLoop().RunUntilIdle();

    // The client should be connected.
    EXPECT_TRUE(
        client.CheckReceivedOnConnected({} /* expected_used_features */));

    // Close the client. The host should detect that there are no clients left
    // and ask the worker to terminate.
    client.Close();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(worker.CheckReceivedTerminate());

    // Simulate the worker terminating by breaking the Mojo connection when
    // |worker| goes out of scope.
  }
  base::RunLoop().RunUntilIdle();

  // The host should have self-destructed.
  EXPECT_FALSE(host);
}

TEST_F(SharedWorkerHostTest, DestructBeforeStarting) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Add a client.
  MockSharedWorkerClient client;
  blink::mojom::SharedWorkerClientPtr client_ptr;
  client.Bind(mojo::MakeRequest(&client_ptr));
  MessagePortChannel local_port = AddClient(host.get(), std::move(client_ptr));
  base::RunLoop().RunUntilIdle();

  // Destroy the host before starting.
  service_.DestroyHost(host.get());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(host);

  // The client should be told startup failed.
  EXPECT_TRUE(client.CheckReceivedOnScriptLoadFailed());
}

TEST_F(SharedWorkerHostTest, TerminateBeforeStarting) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Add a client.
  MockSharedWorkerClient client;
  blink::mojom::SharedWorkerClientPtr client_ptr;
  client.Bind(mojo::MakeRequest(&client_ptr));
  MessagePortChannel local_port = AddClient(host.get(), std::move(client_ptr));
  base::RunLoop().RunUntilIdle();

  // Request to terminate the worker before starting.
  host->TerminateWorker();
  base::RunLoop().RunUntilIdle();

  // The client should be told startup failed.
  EXPECT_TRUE(client.CheckReceivedOnScriptLoadFailed());

  // The host should be destroyed.
  EXPECT_FALSE(host);
}

TEST_F(SharedWorkerHostTest, TerminateAfterStarting) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Create the factory.
  blink::mojom::SharedWorkerFactoryPtr factory;
  MockSharedWorkerFactory factory_impl(mojo::MakeRequest(&factory));
  // Start the worker.
  StartWorker(host.get(), std::move(factory));

  // Add a client.
  MockSharedWorkerClient client;
  blink::mojom::SharedWorkerClientPtr client_ptr;
  client.Bind(mojo::MakeRequest(&client_ptr));
  MessagePortChannel local_port = AddClient(host.get(), std::move(client_ptr));
  base::RunLoop().RunUntilIdle();

  {
    blink::mojom::SharedWorkerHostPtr worker_host;
    blink::mojom::SharedWorkerRequest worker_request;
    EXPECT_TRUE(factory_impl.CheckReceivedCreateSharedWorker(
        host->instance()->url(), host->instance()->name(),
        host->instance()->content_security_policy_type(), &worker_host,
        &worker_request));
    MockSharedWorker worker(std::move(worker_request));

    // Terminate after starting.
    host->TerminateWorker();
    base::RunLoop().RunUntilIdle();

    EXPECT_TRUE(worker.CheckReceivedTerminate());
    // We simulate the worker terminating by breaking the Mojo connection when
    // it goes out of scope.
  }

  // Simulate the worker in the renderer terminating.
  base::RunLoop().RunUntilIdle();

  // The client should not have been told startup failed.
  EXPECT_FALSE(client.CheckReceivedOnScriptLoadFailed());
  // The host should be destroyed.
  EXPECT_FALSE(host);
}

TEST_F(SharedWorkerHostTest, OnContextClosed) {
  base::WeakPtr<SharedWorkerHost> host = CreateHost();

  // Start the worker.
  blink::mojom::SharedWorkerFactoryPtr factory;
  MockSharedWorkerFactory factory_impl(mojo::MakeRequest(&factory));
  StartWorker(host.get(), std::move(factory));

  // Add a client.
  MockSharedWorkerClient client;
  blink::mojom::SharedWorkerClientPtr client_ptr;
  client.Bind(mojo::MakeRequest(&client_ptr));
  MessagePortChannel local_port = AddClient(host.get(), std::move(client_ptr));
  base::RunLoop().RunUntilIdle();

  {
    blink::mojom::SharedWorkerHostPtr worker_host;
    blink::mojom::SharedWorkerRequest worker_request;
    EXPECT_TRUE(factory_impl.CheckReceivedCreateSharedWorker(
        host->instance()->url(), host->instance()->name(),
        host->instance()->content_security_policy_type(), &worker_host,
        &worker_request));
    MockSharedWorker worker(std::move(worker_request));

    // Simulate the worker calling OnContextClosed().
    worker_host->OnContextClosed();
    base::RunLoop().RunUntilIdle();

    // Close the client. The host should detect that there are no clients left
    // and terminate the worker.
    client.Close();
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(worker.CheckReceivedTerminate());

    // Simulate the worker terminating by breaking the Mojo connection when
    // |worker| goes out of scope.
  }
  base::RunLoop().RunUntilIdle();

  // The host should have self-destructed.
  EXPECT_FALSE(host);
}

}  // namespace content
