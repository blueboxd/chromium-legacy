// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/dedicated_worker_service_impl.h"

#include <memory>

#include "base/run_loop.h"
#include "base/scoped_observer.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/worker_host/dedicated_worker_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/worker/dedicated_worker_host_factory.mojom.h"
#include "third_party/blink/public/mojom/worker/worker_main_script_load_params.mojom.h"

namespace content {

// Mocks a dedicated worker living in a renderer process.
class MockDedicatedWorker
    : public blink::mojom::DedicatedWorkerHostFactoryClient {
 public:
  MockDedicatedWorker(int worker_process_id,
                      GlobalFrameRoutingId render_frame_host_id) {
    CreateDedicatedWorkerHostFactory(worker_process_id, render_frame_host_id,
                                     render_frame_host_id, url::Origin(),
                                     factory_.BindNewPipeAndPassReceiver());

    if (base::FeatureList::IsEnabled(blink::features::kPlzDedicatedWorker)) {
      factory_->CreateWorkerHostAndStartScriptLoad(
          GURL(), url::Origin(), network::mojom::CredentialsMode::kSameOrigin,
          blink::mojom::FetchClientSettingsObject::New(),
          mojo::PendingRemote<blink::mojom::BlobURLToken>(),
          receiver_.BindNewPipeAndPassRemote(),
          remote_host_.BindNewPipeAndPassReceiver());
    } else {
      factory_->CreateWorkerHost(
          url::Origin(), browser_interface_broker_.BindNewPipeAndPassReceiver(),
          remote_host_.BindNewPipeAndPassReceiver());
    }
  }

  ~MockDedicatedWorker() override = default;

  // Non-copyable.
  MockDedicatedWorker(const MockDedicatedWorker& other) = delete;

  // blink::mojom::DedicatedWorkerHostFactoryClient:
  void OnWorkerHostCreated(
      mojo::PendingRemote<blink::mojom::BrowserInterfaceBroker>
          browser_interface_broker) override {
    browser_interface_broker_.Bind(std::move(browser_interface_broker));
  }

  void OnScriptLoadStarted(
      blink::mojom::ServiceWorkerProviderInfoForClientPtr
          service_worker_provider_info,
      blink::mojom::WorkerMainScriptLoadParamsPtr main_script_load_params,
      std::unique_ptr<blink::PendingURLLoaderFactoryBundle>
          pending_subresource_loader_factory_bundle,
      mojo::PendingReceiver<blink::mojom::SubresourceLoaderUpdater>
          subresource_loader_updater,
      blink::mojom::ControllerServiceWorkerInfoPtr controller_info) override {}
  void OnScriptLoadStartFailed() override {}

 private:
  // Only used with the kPlzDedicatedWorker feature.
  mojo::Receiver<blink::mojom::DedicatedWorkerHostFactoryClient> receiver_{
      this};

  // Allows creating the dedicated worker host.
  mojo::Remote<blink::mojom::DedicatedWorkerHostFactory> factory_;

  mojo::Remote<blink::mojom::BrowserInterfaceBroker> browser_interface_broker_;
  mojo::Remote<blink::mojom::DedicatedWorkerHost> remote_host_;
};

class DedicatedWorkerServiceImplTest : public RenderViewHostImplTestHarness,
                                       public testing::WithParamInterface<bool>,
                                       public DedicatedWorkerService::Observer {
 public:
  struct DedicatedWorkerInfo {
    int worker_process_id;
    GlobalFrameRoutingId ancestor_render_frame_host_id;
  };

  DedicatedWorkerServiceImplTest() = default;
  ~DedicatedWorkerServiceImplTest() override = default;

  // Non-copyable.
  DedicatedWorkerServiceImplTest(const DedicatedWorkerServiceImplTest& other) =
      delete;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatureState(
        blink::features::kPlzDedicatedWorker, GetParam());
    RenderViewHostImplTestHarness::SetUp();
    browser_context_ = std::make_unique<TestBrowserContext>();
    scoped_dedicated_worker_service_observer_.Add(
        BrowserContext::GetDefaultStoragePartition(browser_context_.get())
            ->GetDedicatedWorkerService());
  }

  void TearDown() override {
    scoped_dedicated_worker_service_observer_.RemoveAll();
    browser_context_ = nullptr;
    RenderViewHostImplTestHarness::TearDown();
  }

  // DedicatedWorkerService::Observer:
  void OnWorkerStarted(
      DedicatedWorkerId dedicated_worker_id,
      int worker_process_id,
      GlobalFrameRoutingId ancestor_render_frame_host_id) override {
    bool inserted =
        dedicated_worker_infos_
            .emplace(dedicated_worker_id,
                     DedicatedWorkerInfo{worker_process_id,
                                         ancestor_render_frame_host_id})
            .second;
    DCHECK(inserted);

    DCHECK(on_worker_event_callback_);
    std::move(on_worker_event_callback_).Run();
  }
  void OnBeforeWorkerTerminated(
      DedicatedWorkerId dedicated_worker_id,
      GlobalFrameRoutingId ancestor_render_frame_host_id) override {
    size_t removed = dedicated_worker_infos_.erase(dedicated_worker_id);
    DCHECK_EQ(removed, 1u);

    DCHECK(on_worker_event_callback_);
    std::move(on_worker_event_callback_).Run();
  }

  void RunUntilWorkerEvent() {
    base::RunLoop run_loop;
    on_worker_event_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  const base::flat_map<DedicatedWorkerId, DedicatedWorkerInfo>&
  dedicated_worker_infos() {
    return dedicated_worker_infos_;
  }

  std::unique_ptr<TestWebContents> CreateWebContents(const GURL& url) {
    std::unique_ptr<TestWebContents> web_contents(TestWebContents::Create(
        browser_context_.get(),
        SiteInstanceImpl::Create(browser_context_.get())));
    web_contents->NavigateAndCommit(url);
    return web_contents;
  }

 private:
  // Controls the state of the blink::features::kPlzDedicatedWorker feature.
  base::test::ScopedFeatureList scoped_feature_list_;

  std::unique_ptr<TestBrowserContext> browser_context_;

  ScopedObserver<DedicatedWorkerService, DedicatedWorkerService::Observer>
      scoped_dedicated_worker_service_observer_{this};

  // Used to wait until one of OnWorkerStarted() or OnBeforeWorkerTerminated()
  // is called.
  base::OnceClosure on_worker_event_callback_;

  base::flat_map<DedicatedWorkerId, DedicatedWorkerInfo>
      dedicated_worker_infos_;
};

TEST_P(DedicatedWorkerServiceImplTest, ObserveWorkerCreationAndDestruction) {
  std::unique_ptr<TestWebContents> web_contents =
      CreateWebContents(GURL("http://example.com/"));
  TestRenderFrameHost* render_frame_host = web_contents->GetMainFrame();

  // At first, there is no live dedicated worker.
  EXPECT_TRUE(dedicated_worker_infos().empty());

  // Create the dedicated worker.
  GlobalFrameRoutingId ancestor_render_frame_host_id =
      render_frame_host->GetGlobalFrameRoutingId();
  auto mock_dedicated_worker = std::make_unique<MockDedicatedWorker>(
      render_frame_host->GetProcess()->GetID(), ancestor_render_frame_host_id);
  RunUntilWorkerEvent();

  // The service sent a OnWorkerStarted() notification.
  ASSERT_EQ(dedicated_worker_infos().size(), 1u);
  const DedicatedWorkerInfo& dedicated_worker_info =
      dedicated_worker_infos().begin()->second;
  EXPECT_EQ(dedicated_worker_info.worker_process_id,
            render_frame_host->GetProcess()->GetID());
  EXPECT_EQ(dedicated_worker_info.ancestor_render_frame_host_id,
            ancestor_render_frame_host_id);

  // Delete the dedicated worker.
  mock_dedicated_worker = nullptr;
  RunUntilWorkerEvent();

  // The service sent a OnBeforeWorkerTerminated() notification.
  EXPECT_TRUE(dedicated_worker_infos().empty());
}

// Runs DedicatedWorkerServiceImplTest with both the enabled and disabled state
// of the kPlzDedicatedWorker feature.
INSTANTIATE_TEST_SUITE_P(, DedicatedWorkerServiceImplTest, testing::Bool());

}  // namespace content
