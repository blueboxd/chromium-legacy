// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/worker_main_script_loader.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/http/http_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-shared.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader_options.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/worker_main_script_loader_client.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"

namespace blink {

namespace {

const char kTopLevelScriptURL[] = "https://example.com/worker.js";
const char kHeader[] =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/javascript\n\n";
const char kFailHeader[] = "HTTP/1.1 404 Not Found\n\n";
const std::string kTopLevelScript = "fetch(\"empty.html\");";

class WorkerMainScriptLoaderTest : public testing::Test {
 public:
  WorkerMainScriptLoaderTest()
      : fake_loader_(pending_remote_loader_.InitWithNewPipeAndPassReceiver()),
        client_(MakeGarbageCollected<TestClient>()) {
    scoped_feature_list_.InitWithFeatures(
        {blink::features::kLoadMainScriptForPlzDedicatedWorkerByParams,
         blink::features::kPlzDedicatedWorker},
        {});
  }

 protected:
  class TestPlatform final : public TestingPlatformSupport {
   public:
    void PopulateURLResponse(const WebURL& url,
                             const network::mojom::URLResponseHead& head,
                             WebURLResponse* response,
                             bool report_security_info,
                             int request_id) override {
      response->SetCurrentRequestUrl(url);
      response->SetHttpStatusCode(head.headers.get()->response_code());
      response->SetMimeType(WebString::FromUTF8(head.mime_type));
      response->SetTextEncodingName(WebString::FromUTF8(head.charset));
    }
  };

  class TestClient final : public GarbageCollected<TestClient>,
                           public WorkerMainScriptLoaderClient {

   public:
    // Implements WorkerMainScriptLoaderClient.
    void DidReceiveData(base::span<const char> data) override {
      if (!data_)
        data_ = SharedBuffer::Create(data.data(), data.size());
      else
        data_->Append(data.data(), data.size());
    }
    void OnFinishedLoadingWorkerMainScript() override { finished_ = true; }
    void OnFailedLoadingWorkerMainScript() override { failed_ = true; }

    bool LoadingIsFinished() const { return finished_; }
    bool LoadingIsFailed() const { return failed_; }

    SharedBuffer* Data() const { return data_.get(); }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(worker_main_script_loader_);
    }

   private:
    Member<WorkerMainScriptLoader> worker_main_script_loader_;
    scoped_refptr<SharedBuffer> data_;
    bool finished_ = false;
    bool failed_ = false;
  };

  class FakeURLLoader final : public network::mojom::URLLoader {
   public:
    explicit FakeURLLoader(
        mojo::PendingReceiver<network::mojom::URLLoader> url_loader_receiver)
        : receiver_(this, std::move(url_loader_receiver)) {}
    ~FakeURLLoader() override = default;

    FakeURLLoader(const FakeURLLoader&) = delete;
    FakeURLLoader& operator=(const FakeURLLoader&) = delete;

    // network::mojom::URLLoader overrides.
    void FollowRedirect(const std::vector<std::string>&,
                        const net::HttpRequestHeaders&,
                        const net::HttpRequestHeaders&,
                        const base::Optional<GURL>&) override {}
    void SetPriority(net::RequestPriority priority,
                     int32_t intra_priority_value) override {}
    void PauseReadingBodyFromNet() override {}
    void ResumeReadingBodyFromNet() override {}

   private:
    mojo::Receiver<network::mojom::URLLoader> receiver_;
  };

  class FakeResourceLoadInfoNotifier final
      : public blink::mojom::ResourceLoadInfoNotifier {
   public:
    explicit FakeResourceLoadInfoNotifier(
        mojo::PendingReceiver<blink::mojom::ResourceLoadInfoNotifier> receiver)
        : receiver_(this, std::move(receiver)) {}

    FakeResourceLoadInfoNotifier(const FakeResourceLoadInfoNotifier&) = delete;
    FakeResourceLoadInfoNotifier& operator=(
        const FakeResourceLoadInfoNotifier&) = delete;

    // blink::mojom::ResourceLoadInfoNotifier overrides.
    void NotifyResourceRedirectReceived(
        const net::RedirectInfo& redirect_info,
        network::mojom::URLResponseHeadPtr redirect_response) override {}
    void NotifyResourceResponseReceived(
        blink::mojom::ResourceLoadInfoPtr resource_load_info,
        network::mojom::URLResponseHeadPtr head,
        int32_t previews_state) override {
      resource_load_info_ = std::move(resource_load_info);
    }
    void NotifyResourceTransferSizeUpdated(
        int32_t request_id,
        int32_t transfer_size_diff) override {}
    void NotifyResourceLoadCompleted(
        blink::mojom::ResourceLoadInfoPtr resource_load_info,
        const ::network::URLLoaderCompletionStatus& status) override {}
    void NotifyResourceLoadCanceled(int32_t request_id) override {}
    void Clone(mojo::PendingReceiver<blink::mojom::ResourceLoadInfoNotifier>
                   pending_resource_load_info_notifier) override {}

    std::string GetMimeType() { return resource_load_info_->mime_type; }

   private:
    blink::mojom::ResourceLoadInfoPtr resource_load_info_;
    mojo::Receiver<blink::mojom::ResourceLoadInfoNotifier> receiver_;
  };

  MojoCreateDataPipeOptions CreateDataPipeOptions() {
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = 1024;
    return options;
  }

  std::unique_ptr<WorkerMainScriptLoadParameters> CreateMainScriptLoaderParams(
      const char* header,
      mojo::ScopedDataPipeProducerHandle* body_producer) {
    auto head = network::mojom::URLResponseHead::New();
    head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(header));
    head->headers->GetMimeType(&head->mime_type);
    network::mojom::URLLoaderClientEndpointsPtr endpoints =
        network::mojom::URLLoaderClientEndpoints::New(
            std::move(pending_remote_loader_),
            loader_client_.BindNewPipeAndPassReceiver());

    std::unique_ptr<WorkerMainScriptLoadParameters>
        worker_main_script_load_params =
            std::make_unique<WorkerMainScriptLoadParameters>();
    worker_main_script_load_params->response_head = std::move(head);
    worker_main_script_load_params->url_loader_client_endpoints =
        std::move(endpoints);
    mojo::ScopedDataPipeConsumerHandle body_consumer;
    MojoCreateDataPipeOptions options = CreateDataPipeOptions();
    EXPECT_EQ(MOJO_RESULT_OK,
              mojo::CreateDataPipe(&options, body_producer, &body_consumer));
    worker_main_script_load_params->response_body = std::move(body_consumer);

    return worker_main_script_load_params;
  }

  WorkerMainScriptLoader* CreateWorkerMainScriptLoaderAndStartLoading(
      std::unique_ptr<WorkerMainScriptLoadParameters>
          worker_main_script_load_params,
      mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
          pending_remote) {
    WorkerMainScriptLoader* worker_main_script_loader =
        MakeGarbageCollected<WorkerMainScriptLoader>();
    worker_main_script_loader->Start(
        KURL(kTopLevelScriptURL), std::move(worker_main_script_load_params),
        options_, mojom::RequestContextType::SHARED_WORKER,
        network::mojom::RequestDestination::kSharedWorker,
        MakeGarbageCollected<MockFetchContext>(),
        blink::CrossVariantMojoRemote<
            blink::mojom::ResourceLoadInfoNotifierInterfaceBase>(
            std::move(pending_remote)),
        client_);
    return worker_main_script_loader;
  }

  void Complete(int net_error) {
    loader_client_->OnComplete(network::URLLoaderCompletionStatus(net_error));
    base::RunLoop().RunUntilIdle();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  ScopedTestingPlatformSupport<TestPlatform> platform_;

  mojo::PendingRemote<network::mojom::URLLoader> pending_remote_loader_;
  mojo::Remote<network::mojom::URLLoaderClient> loader_client_;
  FakeURLLoader fake_loader_;

  ResourceLoaderOptions options_;
  Persistent<TestClient> client_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(WorkerMainScriptLoaderTest, ResponseWithSucessThenOnComplete) {
  mojo::ScopedDataPipeProducerHandle body_producer;
  mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
      pending_resource_load_info_notifier;
  FakeResourceLoadInfoNotifier fake_resource_load_info_notifier(
      pending_resource_load_info_notifier.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params =
          CreateMainScriptLoaderParams(kHeader, &body_producer);

  Persistent<WorkerMainScriptLoader> worker_main_script_loader =
      CreateWorkerMainScriptLoaderAndStartLoading(
          std::move(worker_main_script_load_params),
          std::move(pending_resource_load_info_notifier));
  mojo::BlockingCopyFromString(kTopLevelScript, body_producer);
  body_producer.reset();
  Complete(net::OK);

  EXPECT_TRUE(client_->LoadingIsFinished());
  EXPECT_FALSE(client_->LoadingIsFailed());
  EXPECT_EQ(KURL(kTopLevelScriptURL),
            worker_main_script_loader->GetRequestURL());
  EXPECT_EQ(UTF8Encoding(), worker_main_script_loader->GetScriptEncoding());
  EXPECT_EQ(kTopLevelScript,
            std::string(client_->Data()->Data(), client_->Data()->size()));
  EXPECT_EQ("text/javascript", fake_resource_load_info_notifier.GetMimeType());
}

TEST_F(WorkerMainScriptLoaderTest, ResponseWithFailureThenOnComplete) {
  mojo::ScopedDataPipeProducerHandle body_producer;
  mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
      pending_resource_load_info_notifier;
  FakeResourceLoadInfoNotifier fake_resource_load_info_notifier(
      pending_resource_load_info_notifier.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params =
          CreateMainScriptLoaderParams(kFailHeader, &body_producer);

  Persistent<WorkerMainScriptLoader> worker_main_script_loader =
      CreateWorkerMainScriptLoaderAndStartLoading(
          std::move(worker_main_script_load_params),
          std::move(pending_resource_load_info_notifier));
  mojo::BlockingCopyFromString("PAGE NOT FOUND\n", body_producer);
  Complete(net::OK);
  body_producer.reset();

  EXPECT_FALSE(client_->LoadingIsFinished());
  EXPECT_TRUE(client_->LoadingIsFailed());
}

TEST_F(WorkerMainScriptLoaderTest, DisconnectBeforeOnComplete) {
  mojo::ScopedDataPipeProducerHandle body_producer;
  mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
      pending_resource_load_info_notifier;
  FakeResourceLoadInfoNotifier fake_resource_load_info_notifier(
      pending_resource_load_info_notifier.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params =
          CreateMainScriptLoaderParams(kHeader, &body_producer);

  Persistent<WorkerMainScriptLoader> worker_main_script_loader =
      CreateWorkerMainScriptLoaderAndStartLoading(
          std::move(worker_main_script_load_params),
          std::move(pending_resource_load_info_notifier));
  loader_client_.reset();
  body_producer.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(client_->LoadingIsFinished());
  EXPECT_TRUE(client_->LoadingIsFailed());
}

TEST_F(WorkerMainScriptLoaderTest, OnCompleteWithError) {
  mojo::ScopedDataPipeProducerHandle body_producer;
  mojo::PendingRemote<blink::mojom::ResourceLoadInfoNotifier>
      pending_resource_load_info_notifier;
  FakeResourceLoadInfoNotifier fake_resource_load_info_notifier(
      pending_resource_load_info_notifier.InitWithNewPipeAndPassReceiver());
  std::unique_ptr<WorkerMainScriptLoadParameters>
      worker_main_script_load_params =
          CreateMainScriptLoaderParams(kHeader, &body_producer);

  Persistent<WorkerMainScriptLoader> worker_main_script_loader =
      CreateWorkerMainScriptLoaderAndStartLoading(
          std::move(worker_main_script_load_params),
          std::move(pending_resource_load_info_notifier));
  mojo::BlockingCopyFromString(kTopLevelScript, body_producer);
  Complete(net::ERR_FAILED);
  body_producer.reset();

  EXPECT_FALSE(client_->LoadingIsFinished());
  EXPECT_TRUE(client_->LoadingIsFailed());
}

}  // namespace

}  // namespace blink
