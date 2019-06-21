// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/exported/web_embedded_worker_impl.h"

#include <memory>

#include "base/feature_list.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/service_worker/controller_service_worker_mode.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_installed_scripts_manager.mojom-blink.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom-blink.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_client.h"
#include "third_party/blink/public/web/modules/service_worker/web_service_worker_context_proxy.h"
#include "third_party/blink/public/web/web_embedded_worker_start_data.h"
#include "third_party/blink/public/web/web_settings.h"
#include "third_party/blink/renderer/modules/service_worker/service_worker_installed_scripts_manager.h"
#include "third_party/blink/renderer/modules/service_worker/thread_safe_script_container.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_error.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {
namespace {

const char* kNotFoundScriptURL = "https://www.example.com/sw-404.js";

// Fake network provider for service worker execution contexts.
class FakeServiceWorkerNetworkProvider final
    : public WebServiceWorkerNetworkProvider {
 public:
  FakeServiceWorkerNetworkProvider() = default;
  ~FakeServiceWorkerNetworkProvider() override = default;

  void WillSendRequest(blink::WebURLRequest& request) override {}

  // Returns a loader from the mock factory. In production code, this uses the
  // factory provided at worker startup to load non-installed scripts via
  // ServiceWorkerScriptLoaderFactory.
  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const WebURLRequest& request,
      std::unique_ptr<scheduler::WebResourceLoadingTaskRunnerHandle>) override {
    return Platform::Current()->GetURLLoaderMockFactory()->CreateURLLoader(
        nullptr);
  }

  blink::mojom::ControllerServiceWorkerMode GetControllerServiceWorkerMode()
      override {
    return blink::mojom::ControllerServiceWorkerMode::kNoController;
  }

  int64_t ControllerServiceWorkerID() override {
    return mojom::blink::kInvalidServiceWorkerVersionId;
  }

  void DispatchNetworkQuiet() override {}
};

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
      client->DidFinishLoading(base::TimeTicks(), 0, 0, 0, false, {});
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

  MOCK_METHOD0(WorkerReadyForInspectionOnMainThread, void());
  MOCK_METHOD0(WorkerContextFailedToStartOnMainThread, void());
  MOCK_METHOD0(WorkerScriptLoadedOnMainThread, void());

  void WorkerContextStarted(WebServiceWorkerContextProxy* proxy,
                            scoped_refptr<base::SequencedTaskRunner>) override {
    mojom::blink::ServiceWorkerHostAssociatedPtrInfo host_ptr_info;
    auto host_request = mojo::MakeRequest(&host_ptr_info);

    mojom::blink::ServiceWorkerRegistrationObjectHostAssociatedPtrInfo
        registration_object_host_ptr_info;
    auto registration_object_host_request =
        mojo::MakeRequest(&registration_object_host_ptr_info);
    mojom::blink::ServiceWorkerRegistrationObjectAssociatedPtrInfo
        registration_object_ptr_info;

    // Simulates calling blink.mojom.ServiceWorker.InitializeGlobalScope() to
    // unblock the service worker script evaluation.
    mojom::blink::ServiceWorkerPtr service_worker;
    proxy->BindServiceWorker(
        mojo::MakeRequest(&service_worker).PassMessagePipe());
    service_worker->InitializeGlobalScope(
        std::move(host_ptr_info),
        mojom::blink::ServiceWorkerRegistrationObjectInfo::New(
            2 /* registration_id */, KURL("https://example.com"),
            mojom::blink::ServiceWorkerUpdateViaCache::kImports,
            std::move(registration_object_host_ptr_info),
            mojo::MakeRequest(&registration_object_ptr_info), nullptr, nullptr,
            nullptr),
        mojom::blink::FetchHandlerExistence::EXISTS);

    // To make the other side callable.
    mojo::AssociateWithDisconnectedPipe(host_request.PassHandle());
    mojo::AssociateWithDisconnectedPipe(
        registration_object_host_request.PassHandle());
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

  std::unique_ptr<WebServiceWorkerNetworkProvider>
  CreateServiceWorkerNetworkProviderOnMainThread() override {
    return std::make_unique<FakeServiceWorkerNetworkProvider>();
  }

  scoped_refptr<WebWorkerFetchContext>
  CreateWorkerFetchContextOnMainThreadLegacy(
      WebServiceWorkerNetworkProvider* network_provider) override {
    return base::MakeRefCounted<FakeWebWorkerFetchContext>();
  }

  scoped_refptr<WebWorkerFetchContext> CreateWorkerFetchContextOnMainThread()
      override {
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

class MockServiceWorkerInstalledScriptsManager
    : public ServiceWorkerInstalledScriptsManager {
 public:
  MockServiceWorkerInstalledScriptsManager()
      : ServiceWorkerInstalledScriptsManager(
            Vector<KURL>() /* installed_urls */,
            mojom::blink::ServiceWorkerInstalledScriptsManagerRequest(
                mojo::MessagePipe().handle1),
            mojom::blink::ServiceWorkerInstalledScriptsManagerHostPtrInfo(
                mojo::MessagePipe().handle0,
                mojom::blink::ServiceWorkerInstalledScriptsManagerHost::
                    Version_),
            // Pass a temporary task runner to ensure
            // ServiceWorkerInstalledScriptsManager construction succeeds.
            Platform::Current()
                ->CreateThread(ThreadCreationParams(WebThreadType::kTestThread)
                                   .SetThreadNameForTest("io thread"))
                ->GetTaskRunner()) {}
  MOCK_CONST_METHOD1(IsScriptInstalled, bool(const KURL& script_url));
  MOCK_METHOD1(GetRawScriptData,
               std::unique_ptr<ThreadSafeScriptContainer::RawScriptData>(
                   const KURL& script_url));
};

class WebEmbeddedWorkerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    auto installed_scripts_manager =
        std::make_unique<MockServiceWorkerInstalledScriptsManager>();
    mock_installed_scripts_manager_ = installed_scripts_manager.get();
    mock_client_ = std::make_unique<MockServiceWorkerContextClient>();
    worker_ = WebEmbeddedWorkerImpl::CreateForTesting(
        mock_client_.get(), std::move(installed_scripts_manager));

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
    start_data_.pause_after_download_mode =
        WebEmbeddedWorkerStartData::kDontPauseAfterDownload;
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
  MockServiceWorkerInstalledScriptsManager* mock_installed_scripts_manager_;
  std::unique_ptr<WebEmbeddedWorkerImpl> worker_;
};

}  // namespace

TEST_F(WebEmbeddedWorkerImplTest, TerminateSoonAfterStart) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  EXPECT_CALL(*mock_client_, WorkerContextFailedToStartOnMainThread()).Times(1);
  worker_->TerminateWorkerContext();
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateSoonAfterStart_OMT_Fetch) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  worker_->TerminateWorkerContext();
  // The worker thread was started. Wait for shutdown tasks to finish.
  worker_->WaitForShutdownForTesting();
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileWaitingForDebugger) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  start_data_.wait_for_debugger_mode =
      WebEmbeddedWorkerStartData::kWaitForDebugger;
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  EXPECT_CALL(*mock_client_, WorkerContextFailedToStartOnMainThread()).Times(1);
  worker_->TerminateWorkerContext();
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileWaitingForDebugger_OMT_Fetch) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  start_data_.wait_for_debugger_mode =
      WebEmbeddedWorkerStartData::kWaitForDebugger;
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  worker_->TerminateWorkerContext();
  // The worker thread isn't started yet so we don't have to wait for shutdown.
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileLoadingScript) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Terminate before loading the script.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStartOnMainThread()).Times(1);
  worker_->TerminateWorkerContext();
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
}

TEST_F(WebEmbeddedWorkerImplTest, TerminateWhileLoadingScript_OMT_Fetch) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Terminate before finishing the script load.
  worker_->TerminateWorkerContext();
  // The worker thread was started. Wait for shutdown tasks to finish.
  worker_->WaitForShutdownForTesting();
}

// Tests terminating worker context between script download and execution.
// No "OMT_Fetch" variant for this test because "pause after download" doesn't
// have effect for off-main-thread script fetch. When off-main-thread fetch
// is on, pausing/resuming is handled in ServiceWorkerGlobalScope.
TEST_F(WebEmbeddedWorkerImplTest, TerminateWhilePausedAfterDownload) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  // Load the shadow page.
  start_data_.pause_after_download_mode =
      WebEmbeddedWorkerStartData::kPauseAfterDownload;
  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoadedOnMainThread()).Times(1);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  // Terminate before resuming after download.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStartOnMainThread()).Times(1);
  worker_->TerminateWorkerContext();
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
}

TEST_F(WebEmbeddedWorkerImplTest, ScriptNotFound) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  // Load the shadow page.
  WebURL script_url = url_test_helpers::ToKURL(kNotFoundScriptURL);
  WebURLResponse response;
  response.SetMimeType("text/javascript");
  response.SetHttpStatusCode(404);
  ResourceError error = ResourceError::Failure(script_url);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterErrorURL(
      script_url, response, error);
  start_data_.script_url = script_url;
  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoadedOnMainThread()).Times(0);
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStartOnMainThread()).Times(1);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
}

TEST_F(WebEmbeddedWorkerImplTest, ScriptNotFound_OMT_Fetch) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndEnableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  // Load the shadow page.
  WebURL script_url = url_test_helpers::ToKURL(kNotFoundScriptURL);
  WebURLResponse response;
  response.SetMimeType("text/javascript");
  response.SetHttpStatusCode(404);
  ResourceError error = ResourceError::Failure(script_url);
  Platform::Current()->GetURLLoaderMockFactory()->RegisterErrorURL(
      script_url, response, error);
  start_data_.script_url = script_url;
  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  // Start worker and load the script.
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  mock_client_->WaitUntilFailedToLoadClassicScript();

  // The worker thread was started. Ask to shutdown and wait for shutdown
  // tasks to finish.
  worker_->TerminateWorkerContext();
  worker_->WaitForShutdownForTesting();
}

// The running worker is detected as a memory leak. crbug.com/586897 and
// crbug.com/807754.
// No "OMT_Fetch" variant. See comments on TerminateWhilePausedAfterDownload.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DontPauseAfterDownload DISABLED_DontPauseAfterDownload
#else
#define MAYBE_DontPauseAfterDownload DontPauseAfterDownload
#endif
TEST_F(WebEmbeddedWorkerImplTest, MAYBE_DontPauseAfterDownload) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoadedOnMainThread()).Times(1);
  // This is called on the worker thread.
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  mock_client_->WaitUntilScriptEvaluated();
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Terminate the running worker thread.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStartOnMainThread()).Times(0);
  worker_->TerminateWorkerContext();
  mock_client_->WaitUntilThreadTermination();
}

// The running worker is detected as a memory leak. crbug.com/586897 and
// crbug.com/807754.
// No "OMT_Fetch" variant. See comments on TerminateWhilePausedAfterDownload.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_PauseAfterDownload DISABLED_PauseAfterDownload
#else
#define MAYBE_PauseAfterDownload PauseAfterDownload
#endif
TEST_F(WebEmbeddedWorkerImplTest, MAYBE_PauseAfterDownload) {
  base::test::ScopedFeatureList scoped_list;
  scoped_list.InitAndDisableFeature(
      features::kOffMainThreadServiceWorkerScriptFetch);

  // Load the shadow page.
  EXPECT_CALL(*mock_client_, WorkerReadyForInspectionOnMainThread()).Times(1);
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  start_data_.pause_after_download_mode =
      WebEmbeddedWorkerStartData::kPauseAfterDownload;
  worker_->StartWorkerContext(start_data_);
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Load the script.
  EXPECT_CALL(*mock_client_, WorkerScriptLoadedOnMainThread()).Times(1);
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());

  // Resume after download.
  // This is called on the worker thread.
  EXPECT_CALL(*mock_installed_scripts_manager_,
              IsScriptInstalled(KURL(start_data_.script_url)))
      .Times(testing::AtLeast(1))
      .WillRepeatedly(testing::Return(false));
  worker_->ResumeAfterDownload();
  mock_client_->WaitUntilScriptEvaluated();
  testing::Mock::VerifyAndClearExpectations(mock_client_.get());
  testing::Mock::VerifyAndClearExpectations(mock_installed_scripts_manager_);

  // Terminate the running worker thread.
  EXPECT_CALL(*mock_client_, WorkerContextFailedToStartOnMainThread()).Times(0);
  worker_->TerminateWorkerContext();
  mock_client_->WaitUntilThreadTermination();
}

}  // namespace blink
