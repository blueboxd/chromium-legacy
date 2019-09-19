// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "third_party/blink/public/web/web_embedded_worker_start_data.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace {

const char* kNotFoundScriptURL = "https://www.example.com/sw-404.js";

// A fake WebURLLoader which is used for off-main-thread script fetch tests.
class FakeWebURLLoader final : public WebURLLoader {
 public:
  FakeWebURLLoader() {}
  ~FakeWebURLLoader() override = default;

  void LoadSynchronously(const WebURLRequest&,
                         WebURLLoaderClient*,
                         WebURLResponse&,
                         base::Optional<WebURLError>&,
                         WebData&,
                         int64_t& encoded_data_length,
                         int64_t& encoded_body_length,
                         WebBlobInfo& downloaded_blob) override {
    NOTREACHED();
  }

  void LoadAsynchronously(const WebURLRequest& request,
                          WebURLLoaderClient* client) override {
    if (request.Url().GetString() == kNotFoundScriptURL) {
      WebURLResponse response;
      response.SetMimeType("text/javascript");
      response.SetHttpStatusCode(404);
      client->DidReceiveResponse(response);
      client->DidFinishLoading(base::TimeTicks(), 0, 0, 0, false);
      return;
    }
    // Don't handle other requests intentionally to emulate ongoing load.
  }

  void SetDefersLoading(bool defers) override {}
  void DidChangePriority(WebURLRequest::Priority, int) override {}
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner() override {
    return base::MakeRefCounted<scheduler::FakeTaskRunner>();
  }
};

// A fake WebURLLoaderFactory which is used for off-main-thread script fetch
// tests.
class FakeWebURLLoaderFactory final : public WebURLLoaderFactory {
 public:
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest&,
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>) override {
    return std::make_unique<FakeWebURLLoader>();
  }
};

// A fake WebWorkerFetchContext which is used for off-main-thread script fetch
// tests.
class FakeWebWorkerFetchContext final : public WebWorkerFetchContext {
 public:
  void SetTerminateSyncLoadEvent(base::WaitableEvent*) override {}
  void InitializeOnWorkerThread(AcceptLanguagesWatcher*) override {}
  WebURLLoaderFactory* GetURLLoaderFactory() override {
    return &fake_web_url_loader_factory_;
  }
  std::unique_ptr<WebURLLoaderFactory> WrapURLLoaderFactory(
      mojo::ScopedMessagePipeHandle url_loader_factory_handle) override {
    return nullptr;
  }
  void WillSendRequest(WebURLRequest&) override {}
  mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      const override {
    return mojom::ControllerServiceWorkerMode::kNoController;
  }
  WebURL SiteForCookies() const override { return WebURL(); }
  base::Optional<WebSecurityOrigin> TopFrameOrigin() const override {
    return base::Optional<WebSecurityOrigin>();
  }
  WebString GetAcceptLanguages() const override { return WebString(); }

 private:
  FakeWebURLLoaderFactory fake_web_url_loader_factory_;
};

class MockServiceWorkerContextClient final
    : public WebServiceWorkerContextClient {
 public:
  MockServiceWorkerContextClient() = default;
  ~MockServiceWorkerContextClient() override = default;

  MOCK_METHOD2(WorkerReadyForInspectionOnInitiatorThread,
               void(mojo::ScopedMessagePipeHandle,
                    mojo::ScopedMessagePipeHandle));
  MOCK_METHOD0(WorkerContextFailedToStartOnInitiatorThread, void());

  void WorkerContextStarted(WebServiceWorkerContextProxy* proxy,
                            scoped_refptr<base::SequencedTaskRunner>) override {
    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerHost> host_remote;
    auto host_receiver = host_remote.InitWithNewEndpointAndPassReceiver();

    mojo::PendingAssociatedRemote<
        mojom::blink::ServiceWorkerRegistrationObjectHost>
        registration_object_host;
    auto registration_object_host_receiver =
        registration_object_host.InitWithNewEndpointAndPassReceiver();
    mojo::PendingAssociatedRemote<mojom::blink::ServiceWorkerRegistrationObject>
        registration_object;

    // Simulates calling blink.mojom.ServiceWorker.InitializeGlobalScope() to
    // unblock the service worker script evaluation.
    mojo::Remote<mojom::blink::ServiceWorker> service_worker;
    proxy->BindServiceWorker(
        service_worker.BindNewPipeAndPassReceiver().PassPipe());
    service_worker->InitializeGlobalScope(
        std::move(host_remote),
        mojom::blink::ServiceWorkerRegistrationObjectInfo::New(
            2 /* registration_id */, KURL("https://example.com"),
            mojom::blink::ServiceWorkerUpdateViaCache::kImports,
            std::move(registration_object_host),
            registration_object.InitWithNewEndpointAndPassReceiver(), nullptr,
            nullptr, nullptr),
        mojom::blink::FetchHandlerExistence::EXISTS);

    // To make the other side callable.
    mojo::AssociateWithDisconnectedPipe(host_receiver.PassHandle());
    mojo::AssociateWithDisconnectedPipe(
        registration_object_host_receiver.PassHandle());
  }

  void FailedToLoadClassicScript() override {
    // off-main-script fetch:
    // In production code, calling FailedToLoadClassicScript results in
    // terminating the worker.
    classic_script_load_failure_event_.Signal();
  }

  void DidEvaluateScript(bool /* success */) override {
    script_evaluated_event_.Signal();
  }

  scoped_refptr<WebWorkerFetchContext>
  CreateWorkerFetchContextOnInitiatorThread() override {
    return base::MakeRefCounted<FakeWebWorkerFetchContext>();
  }

  void WorkerContextDestroyed() override { termination_event_.Signal(); }

  // These methods must be called on the main thread.
  void WaitUntilScriptEvaluated() { script_evaluated_event_.Wait(); }
  void WaitUntilThreadTermination() { termination_event_.Wait(); }
  void WaitUntilFailedToLoadClassicScript() {
    classic_script_load_failure_event_.Wait();
  }

 private:
  base::WaitableEvent script_evaluated_event_;
  base::WaitableEvent termination_event_;
  base::WaitableEvent classic_script_load_failure_event_;
};

class WebEmbeddedWorkerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    mock_client_ = std::make_unique<MockServiceWorkerContextClient>();
    worker_ = WebEmbeddedWorkerImpl::CreateForTesting(mock_client_.get());

    WebURL script_url =
        url_test_helpers::ToKURL("https://www.example.com/sw.js");
    WebURLResponse response(script_url);
    response.SetMimeType("text/javascript");
    response.SetHttpStatusCode(200);
    Platform::Current()->GetURLLoaderMockFactory()->RegisterURL(script_url,
                                                                response, "");

    start_data_.script_url = script_url;
    start_data_.user_agent = WebString("dummy user agent");
    start_data_.script_type = mojom::ScriptType::kClassic;
    start_data_.wait_for_debugger_mode =
        WebEmbeddedWorkerStartData::kDontWaitForDebugger;
    start_data_.v8_cache_options = WebSettings::V8CacheOptions::kDefault;
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  WebEmbeddedWorkerStartData start_data_;
  std::unique_ptr<MockServiceWorkerContextClient> mock_client_;
  std::unique_ptr<WebEmbeddedWorkerImpl> worker_;
};

}  // namespace

TEST_F(WebEmbeddedWorkerImplTest, TerminateSoonAfterStart) {
  worker_->StartWorkerContext(
      start_data_,
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::ScopedMessagePipeHandle(),
      Thread::Current()->GetTaskRunner());
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  worker_->TerminateWorkerContext();
  // The worker thread was started. Wait for shutdown tasks to finish.
  worker_->WaitForShutdownForTesting();
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileWaitingForDebugger) {
  start_data_.wait_for_debugger_mode =
      WebEmbeddedWorkerStartData::kWaitForDebugger;
  worker_->StartWorkerContext(
      start_data_,
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::ScopedMessagePipeHandle(),
      Thread::Current()->GetTaskRunner());
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  worker_->TerminateWorkerContext();
  worker_->WaitForShutdownForTesting();
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileLoadingScript) {
  // Load the shadow page.
  worker_->StartWorkerContext(
      start_data_,
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::ScopedMessagePipeHandle(),
      Thread::Current()->GetTaskRunner());
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  // Terminate before finishing the script load.
  worker_->TerminateWorkerContext();
  // The worker thread was started. Wait for shutdown tasks to finish.
  worker_->WaitForShutdownForTesting();
}

TEST_F(WebEmbeddedWorkerImplTest, ScriptNotFound) {
  // Load the shadow page.
  WebURL script_url = url_test_helpers::ToKURL(kNotFoundScriptURL);
  WebURLResponse response;
  response.SetMimeType("text/javascript");
  response.SetHttpStatusCode(404);
  ResourceError error = ResourceError::Failure(script_url);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterErrorURL(
      script_url, response, error);
  start_data_.script_url = script_url;

  // Start worker and load the script.
  worker_->StartWorkerContext(
      start_data_,
      /*installed_scripts_manager_params=*/nullptr,
      /*content_settings_proxy=*/mojo::ScopedMessagePipeHandle(),
      Thread::Current()->GetTaskRunner());
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  mock_client_->WaitUntilFailedToLoadClassicScript();

  // The worker thread was started. Ask to shutdown and wait for shutdown
  // tasks to finish.
  worker_->TerminateWorkerContext();
  worker_->WaitForShutdownForTesting();
}

}  // namespace blink
