// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/shared_worker_service_impl.h"

#include <memory>
#include <set>
#include <string>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/containers/queue.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/worker_host/mock_shared_worker.h"
#include "content/browser/worker_host/shared_worker_connector_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "content/test/not_implemented_network_url_loader_factory.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/worker/shared_worker_info.mojom.h"

using blink::MessagePortChannel;

namespace content {

namespace {

void ConnectToSharedWorker(blink::mojom::SharedWorkerConnectorPtr connector,
                           const GURL& url,
                           const std::string& name,
                           MockSharedWorkerClient* client,
                           MessagePortChannel* local_port) {
  blink::mojom::SharedWorkerInfoPtr info(blink::mojom::SharedWorkerInfo::New(
      url, name, std::string(),
      blink::mojom::ContentSecurityPolicyType::kReport,
      blink::mojom::IPAddressSpace::kPublic));

  mojo::MessagePipe message_pipe;
  *local_port = MessagePortChannel(std::move(message_pipe.handle0));

  blink::mojom::SharedWorkerClientPtr client_proxy;
  client->Bind(mojo::MakeRequest(&client_proxy));

  connector->Connect(std::move(info),
                     blink::mojom::FetchClientSettingsObject::New(),
                     std::move(client_proxy),
                     blink::mojom::SharedWorkerCreationContextType::kSecure,
                     std::move(message_pipe.handle1), nullptr);
}

// Helper to delete the given WebContents and shut down its process. This is
// useful because if FastShutdownIfPossible() is called without deleting the
// WebContents first, shutdown does not actually start.
void KillProcess(std::unique_ptr<WebContents> web_contents) {
  RenderFrameHost* frame_host = web_contents->GetMainFrame();
  RenderProcessHost* process_host = frame_host->GetProcess();
  web_contents.reset();
  process_host->FastShutdownIfPossible(/*page_count=*/0,
                                       /*skip_unload_handlers=*/true);
  ASSERT_TRUE(process_host->FastShutdownStarted());
}

}  // namespace

class SharedWorkerServiceImplTest : public RenderViewHostImplTestHarness {
 public:
  blink::mojom::SharedWorkerConnectorPtr MakeSharedWorkerConnector(
      RenderProcessHost* process_host,
      int frame_id) {
    blink::mojom::SharedWorkerConnectorPtr connector;
    SharedWorkerConnectorImpl::Create(process_host->GetID(), frame_id,
                                      mojo::MakeRequest(&connector));
    return connector;
  }

  // Waits until a SharedWorkerFactoryRequest from the given process is
  // received. kInvalidUniqueID means any process.
  blink::mojom::SharedWorkerFactoryRequest WaitForFactoryRequest(
      int process_id) {
    if (CheckNotReceivedFactoryRequest(process_id)) {
      base::RunLoop run_loop;
      factory_request_callback_ = run_loop.QuitClosure();
      factory_request_callback_process_id_ = process_id;
      run_loop.Run();
    }
    auto iter = (process_id == ChildProcessHost::kInvalidUniqueID)
                    ? received_factory_requests_.begin()
                    : received_factory_requests_.find(process_id);
    DCHECK(iter != received_factory_requests_.end());
    auto& queue = iter->second;
    DCHECK(!queue.empty());
    auto rv = std::move(queue.front());
    queue.pop();
    if (queue.empty())
      received_factory_requests_.erase(iter);
    return rv;
  }

  bool CheckNotReceivedFactoryRequest(int process_id) {
    if (process_id == ChildProcessHost::kInvalidUniqueID)
      return received_factory_requests_.empty();
    return !base::Contains(received_factory_requests_, process_id);
  }

  // Receives a SharedWorkerFactoryRequest.
  void BindSharedWorkerFactory(int process_id,
                               mojo::ScopedMessagePipeHandle handle) {
    if (factory_request_callback_ &&
        (factory_request_callback_process_id_ == process_id ||
         factory_request_callback_process_id_ ==
             ChildProcessHost::kInvalidUniqueID)) {
      factory_request_callback_process_id_ = ChildProcessHost::kInvalidUniqueID;
      std::move(factory_request_callback_).Run();
    }

    if (!base::Contains(received_factory_requests_, process_id)) {
      received_factory_requests_.emplace(
          process_id, base::queue<blink::mojom::SharedWorkerFactoryRequest>());
    }
    received_factory_requests_[process_id].emplace(std::move(handle));
  }

  std::unique_ptr<TestWebContents> CreateWebContents(const GURL& url) {
    std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get())));
    web_contents->NavigateAndCommit(url);
    return web_contents;
  }

 protected:
  SharedWorkerServiceImplTest() : browser_context_(new TestBrowserContext()) {}

  void SetUp() override {
    RenderViewHostImplTestHarness::SetUp();
    render_process_host_factory_ =
        std::make_unique<MockRenderProcessHostFactory>();
    RenderProcessHostImpl::set_render_process_host_factory_for_testing(
        render_process_host_factory_.get());

    fake_url_loader_factory_ = std::make_unique<FakeNetworkURLLoaderFactory>();
    url_loader_factory_wrapper_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            fake_url_loader_factory_.get());
    static_cast<SharedWorkerServiceImpl*>(
        BrowserContext::GetDefaultStoragePartition(browser_context_.get())
            ->GetSharedWorkerService())
        ->SetURLLoaderFactoryForTesting(url_loader_factory_wrapper_);
  }

  void TearDown() override {
    if (url_loader_factory_wrapper_)
      url_loader_factory_wrapper_->Detach();
    browser_context_.reset();
    RenderViewHostImplTestHarness::TearDown();
  }

  std::unique_ptr<TestBrowserContext> browser_context_;

  // Holds received SharedWorkerFactoryRequests for each process.
  base::flat_map<int /* process_id */,
                 base::queue<blink::mojom::SharedWorkerFactoryRequest>>
      received_factory_requests_;

  // The callback is called when a SharedWorkerFactoryRequest for the specified
  // process is received. kInvalidUniqueID means any process.
  base::OnceClosure factory_request_callback_;
  int factory_request_callback_process_id_ = ChildProcessHost::kInvalidUniqueID;

  std::unique_ptr<MockRenderProcessHostFactory> render_process_host_factory_;

  std::unique_ptr<FakeNetworkURLLoaderFactory> fake_url_loader_factory_;
  scoped_refptr<network::WeakWrapperSharedURLLoaderFactory>
      url_loader_factory_wrapper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SharedWorkerServiceImplTest);
};

TEST_F(SharedWorkerServiceImplTest, BasicTest) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetMainFrame();
  MockRenderProcessHost* renderer_host = render_frame_host->GetProcess();
  const int process_id = renderer_host->GetID();
  renderer_host->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id));

  MockSharedWorkerClient client;
  MessagePortChannel local_port;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host, render_frame_host->GetRoutingID()),
                        kUrl, "name", &client, &local_port);

  blink::mojom::SharedWorkerFactoryRequest factory_request =
      WaitForFactoryRequest(process_id);
  MockSharedWorkerFactory factory(std::move(factory_request));
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host;
  blink::mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host, &worker_request));
  MockSharedWorker worker(std::move(worker_request));
  base::RunLoop().RunUntilIdle();

  int connection_request_id;
  MessagePortChannel port;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id, &port));

  EXPECT_TRUE(client.CheckReceivedOnCreated());

  // Simulate events the shared worker would send.
  worker_host->OnReadyForInspection();
  worker_host->OnConnected(connection_request_id);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      client.CheckReceivedOnConnected(std::set<blink::mojom::WebFeature>()));

  // Verify that |port| corresponds to |connector->local_port()|.
  std::string expected_message("test1");
  EXPECT_TRUE(mojo::test::WriteTextMessage(local_port.GetHandle().get(),
                                           expected_message));
  std::string received_message;
  EXPECT_TRUE(
      mojo::test::ReadTextMessage(port.GetHandle().get(), &received_message));
  EXPECT_EQ(expected_message, received_message);

  // Send feature from shared worker to host.
  auto feature1 = static_cast<blink::mojom::WebFeature>(124);
  worker_host->OnFeatureUsed(feature1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client.CheckReceivedOnFeatureUsed(feature1));

  // A message should be sent only one time per feature.
  worker_host->OnFeatureUsed(feature1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client.CheckNotReceivedOnFeatureUsed());

  // Send another feature.
  auto feature2 = static_cast<blink::mojom::WebFeature>(901);
  worker_host->OnFeatureUsed(feature2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client.CheckReceivedOnFeatureUsed(feature2));
}

TEST_F(SharedWorkerServiceImplTest, TwoRendererTest) {
  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  const GURL kUrl("http://example.com/w.js");
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, "name", &client0, &local_port0);

  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerFactoryRequest factory_request =
      WaitForFactoryRequest(process_id0);
  MockSharedWorkerFactory factory(std::move(factory_request));
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host;
  blink::mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, "name", blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host, &worker_request));
  MockSharedWorker worker(std::move(worker_request));
  base::RunLoop().RunUntilIdle();

  int connection_request_id0;
  MessagePortChannel port0;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id0, &port0));

  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Simulate events the shared worker would send.
  worker_host->OnReadyForInspection();
  worker_host->OnConnected(connection_request_id0);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      client0.CheckReceivedOnConnected(std::set<blink::mojom::WebFeature>()));

  // Verify that |port0| corresponds to |connector0->local_port()|.
  std::string expected_message0("test1");
  EXPECT_TRUE(mojo::test::WriteTextMessage(local_port0.GetHandle().get(),
                                           expected_message0));
  std::string received_message0;
  EXPECT_TRUE(
      mojo::test::ReadTextMessage(port0.GetHandle().get(), &received_message0));
  EXPECT_EQ(expected_message0, received_message0);

  auto feature1 = static_cast<blink::mojom::WebFeature>(124);
  worker_host->OnFeatureUsed(feature1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client0.CheckReceivedOnFeatureUsed(feature1));
  auto feature2 = static_cast<blink::mojom::WebFeature>(901);
  worker_host->OnFeatureUsed(feature2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client0.CheckReceivedOnFeatureUsed(feature2));

  // Only a single worker instance in process 0.
  EXPECT_EQ(1u, renderer_host0->GetKeepAliveRefCount());

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, "name", &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Should not have tried to create a new shared worker.
  EXPECT_TRUE(CheckNotReceivedFactoryRequest(process_id1));

  int connection_request_id1;
  MessagePortChannel port1;
  EXPECT_TRUE(worker.CheckReceivedConnect(&connection_request_id1, &port1));

  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Only a single worker instance in process 0.
  EXPECT_EQ(1u, renderer_host0->GetKeepAliveRefCount());
  EXPECT_EQ(0u, renderer_host1->GetKeepAliveRefCount());

  worker_host->OnConnected(connection_request_id1);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(client1.CheckReceivedOnConnected({feature1, feature2}));

  // Verify that |worker_msg_port2| corresponds to |connector1->local_port()|.
  std::string expected_message1("test2");
  EXPECT_TRUE(mojo::test::WriteTextMessage(local_port1.GetHandle().get(),
                                           expected_message1));
  std::string received_message1;
  EXPECT_TRUE(
      mojo::test::ReadTextMessage(port1.GetHandle().get(), &received_message1));
  EXPECT_EQ(expected_message1, received_message1);

  worker_host->OnFeatureUsed(feature1);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client0.CheckNotReceivedOnFeatureUsed());
  EXPECT_TRUE(client1.CheckNotReceivedOnFeatureUsed());

  auto feature3 = static_cast<blink::mojom::WebFeature>(1019);
  worker_host->OnFeatureUsed(feature3);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client0.CheckReceivedOnFeatureUsed(feature3));
  EXPECT_TRUE(client1.CheckReceivedOnFeatureUsed(feature3));
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_NormalCase) {
  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerFactoryRequest factory_request =
      WaitForFactoryRequest(process_id0);
  MockSharedWorkerFactory factory(std::move(factory_request));
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host;
  blink::mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, kName, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host, &worker_request));
  MockSharedWorker worker(std::move(worker_request));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Second client, same worker.

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(CheckNotReceivedFactoryRequest(process_id1));

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_NormalCase_URLMismatch) {
  const GURL kUrl0("http://example.com/w0.js");
  const GURL kUrl1("http://example.com/w1.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl0, kName, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerFactoryRequest factory_request0 =
      WaitForFactoryRequest(process_id0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host0;
  blink::mojom::SharedWorkerRequest worker_request0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl0, kName, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host0, &worker_request0));
  MockSharedWorker worker0(std::move(worker_request0));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Second client, creates worker.

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl1, kName, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerFactoryRequest factory_request1 =
      WaitForFactoryRequest(process_id1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host1;
  blink::mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl1, kName, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host1, &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedTerminate());
  EXPECT_TRUE(worker1.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_NormalCase_NameMismatch) {
  const GURL kUrl("http://example.com/w.js");
  const char kName0[] = "name0";
  const char kName1[] = "name1";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client, creates worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName0, &client0, &local_port0);
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerFactoryRequest factory_request0 =
      WaitForFactoryRequest(process_id0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host0;
  blink::mojom::SharedWorkerRequest worker_request0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName0, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host0, &worker_request0));
  MockSharedWorker worker0(std::move(worker_request0));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Second client, creates worker.

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName1, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerFactoryRequest factory_request1 =
      WaitForFactoryRequest(process_id1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));
  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host1;
  blink::mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName1, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host1, &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedTerminate());
  EXPECT_TRUE(worker1.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_PendingCase) {
  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client and second client are created before the worker starts.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Check that the worker was created. The request could come from either
  // process.

  blink::mojom::SharedWorkerFactoryRequest factory_request =
      WaitForFactoryRequest(ChildProcessHost::kInvalidUniqueID);
  MockSharedWorkerFactory factory(std::move(factory_request));

  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host;
  blink::mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kUrl, kName, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host, &worker_request));
  MockSharedWorker worker(std::move(worker_request));

  base::RunLoop().RunUntilIdle();

  // Check that the worker received two connections.

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_PendingCase_URLMismatch) {
  const GURL kUrl0("http://example.com/w0.js");
  const GURL kUrl1("http://example.com/w1.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), renderer_host0->GetID()));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), renderer_host1->GetID()));

  // First client and second client are created before the workers start.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl0, kName, &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl1, kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Check that both workers were created.

  blink::mojom::SharedWorkerFactoryRequest factory_request0 =
      WaitForFactoryRequest(renderer_host0->GetID());
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  blink::mojom::SharedWorkerFactoryRequest factory_request1 =
      WaitForFactoryRequest(renderer_host1->GetID());
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host0;
  blink::mojom::SharedWorkerRequest worker_request0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl0, kName, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host0, &worker_request0));
  MockSharedWorker worker0(std::move(worker_request0));

  blink::mojom::SharedWorkerHostPtr worker_host1;
  blink::mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl1, kName, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host1, &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));

  base::RunLoop().RunUntilIdle();

  // Check that the workers each received a connection.

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(worker0.CheckNotReceivedConnect());
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(worker1.CheckNotReceivedConnect());
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedTerminate());
  EXPECT_TRUE(worker1.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerTest_PendingCase_NameMismatch) {
  const GURL kUrl("http://example.com/w.js");
  const char kName0[] = "name0";
  const char kName1[] = "name1";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // First client and second client are created before the workers start.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName0, &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName1, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // Check that both workers were created.

  blink::mojom::SharedWorkerFactoryRequest factory_request0 =
      WaitForFactoryRequest(process_id0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  blink::mojom::SharedWorkerFactoryRequest factory_request1 =
      WaitForFactoryRequest(process_id1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host0;
  blink::mojom::SharedWorkerRequest worker_request0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName0, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host0, &worker_request0));
  MockSharedWorker worker0(std::move(worker_request0));

  blink::mojom::SharedWorkerHostPtr worker_host1;
  blink::mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName1, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host1, &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));

  base::RunLoop().RunUntilIdle();

  // Check that the workers each received a connection.

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(worker0.CheckNotReceivedConnect());
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(worker1.CheckNotReceivedConnect());
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedTerminate());
  EXPECT_TRUE(worker1.CheckReceivedTerminate());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest) {
  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // Create three renderer hosts.

  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  std::unique_ptr<TestWebContents> web_contents2 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host2 = web_contents2->GetMainFrame();
  MockRenderProcessHost* renderer_host2 = render_frame_host2->GetProcess();
  const int process_id2 = renderer_host2->GetID();
  renderer_host2->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id2));

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);

  base::RunLoop().RunUntilIdle();

  // Starts a worker.

  blink::mojom::SharedWorkerFactoryRequest factory_request0 =
      WaitForFactoryRequest(process_id0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host0;
  blink::mojom::SharedWorkerRequest worker_request0;
  EXPECT_TRUE(factory0.CheckReceivedCreateSharedWorker(
      kUrl, kName, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host0, &worker_request0));
  MockSharedWorker worker0(std::move(worker_request0));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client0.CheckReceivedOnCreated());

  // Kill this process, which should make worker0 unavailable.
  KillProcess(std::move(web_contents0));

  // Start a new client, attemping to connect to the same worker.
  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // The previous worker is unavailable, so a new worker is created.

  blink::mojom::SharedWorkerFactoryRequest factory_request1 =
      WaitForFactoryRequest(process_id1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host1;
  blink::mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host1, &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker0.CheckNotReceivedConnect());
  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Start another client to confirm that it can connect to the same worker.
  MockSharedWorkerClient client2;
  MessagePortChannel local_port2;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host2, render_frame_host2->GetRoutingID()),
                        kUrl, kName, &client2, &local_port2);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(CheckNotReceivedFactoryRequest(process_id2));

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client2.CheckReceivedOnCreated());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest2) {
  const GURL kUrl("http://example.com/w.js");
  const char kName[] = "name";

  // Create three renderer hosts.

  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  std::unique_ptr<TestWebContents> web_contents2 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host2 = web_contents2->GetMainFrame();
  MockRenderProcessHost* renderer_host2 = render_frame_host2->GetProcess();
  const int process_id2 = renderer_host2->GetID();
  renderer_host2->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id2));

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kUrl, kName, &client0, &local_port0);

  blink::mojom::SharedWorkerFactoryRequest factory_request0 =
      WaitForFactoryRequest(process_id0);
  MockSharedWorkerFactory factory0(std::move(factory_request0));

  // Kill this process, which should make worker0 unavailable.
  KillProcess(std::move(web_contents0));

  // Start a new client, attempting to connect to the same worker.
  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kUrl, kName, &client1, &local_port1);

  base::RunLoop().RunUntilIdle();

  // The previous worker is unavailable, so a new worker is created.

  blink::mojom::SharedWorkerFactoryRequest factory_request1 =
      WaitForFactoryRequest(process_id1);
  MockSharedWorkerFactory factory1(std::move(factory_request1));

  EXPECT_TRUE(
      CheckNotReceivedFactoryRequest(ChildProcessHost::kInvalidUniqueID));

  base::RunLoop().RunUntilIdle();

  blink::mojom::SharedWorkerHostPtr worker_host1;
  blink::mojom::SharedWorkerRequest worker_request1;
  EXPECT_TRUE(factory1.CheckReceivedCreateSharedWorker(
      kUrl, kName, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host1, &worker_request1));
  MockSharedWorker worker1(std::move(worker_request1));

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client1.CheckReceivedOnCreated());

  // Start another client to confirm that it can connect to the same worker.
  MockSharedWorkerClient client2;
  MessagePortChannel local_port2;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host2, render_frame_host2->GetRoutingID()),
                        kUrl, kName, &client2, &local_port2);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(
      CheckNotReceivedFactoryRequest(ChildProcessHost::kInvalidUniqueID));

  EXPECT_TRUE(worker1.CheckReceivedConnect(nullptr, nullptr));
  EXPECT_TRUE(client2.CheckReceivedOnCreated());
}

TEST_F(SharedWorkerServiceImplTest, CreateWorkerRaceTest3) {
  const GURL kURL("http://example.com/w.js");
  const char kName[] = "name";

  // The first renderer host.
  std::unique_ptr<TestWebContents> web_contents0 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host0 = web_contents0->GetMainFrame();
  MockRenderProcessHost* renderer_host0 = render_frame_host0->GetProcess();
  const int process_id0 = renderer_host0->GetID();
  renderer_host0->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id0));

  // The second renderer host.
  std::unique_ptr<TestWebContents> web_contents1 =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host1 = web_contents1->GetMainFrame();
  MockRenderProcessHost* renderer_host1 = render_frame_host1->GetProcess();
  const int process_id1 = renderer_host1->GetID();
  renderer_host1->OverrideBinderForTesting(
      blink::mojom::SharedWorkerFactory::Name_,
      base::BindRepeating(&SharedWorkerServiceImplTest::BindSharedWorkerFactory,
                          base::Unretained(this), process_id1));

  // Both clients try to connect/create a worker.

  MockSharedWorkerClient client0;
  MessagePortChannel local_port0;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host0, render_frame_host0->GetRoutingID()),
                        kURL, kName, &client0, &local_port0);

  MockSharedWorkerClient client1;
  MessagePortChannel local_port1;
  ConnectToSharedWorker(MakeSharedWorkerConnector(
                            renderer_host1, render_frame_host1->GetRoutingID()),
                        kURL, kName, &client1, &local_port1);
  base::RunLoop().RunUntilIdle();

  // Expect a factory request. It can come from either process.
  blink::mojom::SharedWorkerFactoryRequest factory_request =
      WaitForFactoryRequest(ChildProcessHost::kInvalidUniqueID);
  MockSharedWorkerFactory factory(std::move(factory_request));
  base::RunLoop().RunUntilIdle();

  // Expect a create shared worker.
  blink::mojom::SharedWorkerHostPtr worker_host;
  blink::mojom::SharedWorkerRequest worker_request;
  EXPECT_TRUE(factory.CheckReceivedCreateSharedWorker(
      kURL, kName, blink::mojom::ContentSecurityPolicyType::kReport,
      &worker_host, &worker_request));
  MockSharedWorker worker(std::move(worker_request));
  base::RunLoop().RunUntilIdle();

  // Expect one connect for the first client.
  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  client0.CheckReceivedOnCreated();

  // Expect one connect for the second client.
  EXPECT_TRUE(worker.CheckReceivedConnect(nullptr, nullptr));
  client1.CheckReceivedOnCreated();

  // Cleanup

  client0.Close();
  client1.Close();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(worker.CheckReceivedTerminate());
}

}  // namespace content
