// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/service_video_capture_provider.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/mock_callback.h"
#include "base/threading/thread.h"
#include "content/public/browser/video_capture_device_launcher.h"
#include "content/public/browser/video_capture_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/video_capture/public/cpp/mock_push_subscription.h"
#include "services/video_capture/public/cpp/mock_video_capture_service.h"
#include "services/video_capture/public/cpp/mock_video_source.h"
#include "services/video_capture/public/cpp/mock_video_source_provider.h"
#include "services/video_capture/public/mojom/producer.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Mock;
using testing::Invoke;
using testing::_;

namespace content {

static const std::string kStubDeviceId = "StubDevice";
static const media::VideoCaptureParams kArbitraryParams;
static const base::WeakPtr<media::VideoFrameReceiver> kNullReceiver;
static const auto kIgnoreLogMessageCB =
    base::BindRepeating([](const std::string&) {});

class MockVideoCaptureDeviceLauncherCallbacks
    : public VideoCaptureDeviceLauncher::Callbacks {
 public:
  void OnDeviceLaunched(
      std::unique_ptr<LaunchedVideoCaptureDevice> device) override {
    DoOnDeviceLaunched(&device);
  }

  MOCK_METHOD1(DoOnDeviceLaunched,
               void(std::unique_ptr<LaunchedVideoCaptureDevice>* device));
  MOCK_METHOD1(OnDeviceLaunchFailed, void(media::VideoCaptureError error));
  MOCK_METHOD0(OnDeviceLaunchAborted, void());
};

class ServiceVideoCaptureProviderTest : public testing::Test {
 public:
  ServiceVideoCaptureProviderTest()
      : source_provider_binding_(&mock_source_provider_) {
    OverrideVideoCaptureServiceForTesting(&mock_video_capture_service_);
  }

  ~ServiceVideoCaptureProviderTest() override {
    OverrideVideoCaptureServiceForTesting(nullptr);
  }

 protected:
  void SetUp() override {
#if defined(OS_CHROMEOS)
    provider_ = std::make_unique<ServiceVideoCaptureProvider>(
        base::BindRepeating([]() {
          return std::unique_ptr<video_capture::mojom::AcceleratorFactory>();
        }),
        kIgnoreLogMessageCB);
#else
    provider_ =
        std::make_unique<ServiceVideoCaptureProvider>(kIgnoreLogMessageCB);
#endif  // defined(OS_CHROMEOS)

    ON_CALL(mock_video_capture_service_, DoConnectToVideoSourceProvider(_))
        .WillByDefault(Invoke(
            [this](video_capture::mojom::VideoSourceProviderRequest& request) {
              if (source_provider_binding_.is_bound())
                source_provider_binding_.Close();
              source_provider_binding_.Bind(std::move(request));
              wait_for_connection_to_service_.Quit();
            }));

    ON_CALL(mock_source_provider_, DoGetSourceInfos(_))
        .WillByDefault(Invoke([](video_capture::mojom::VideoSourceProvider::
                                     GetSourceInfosCallback& callback) {
          std::vector<media::VideoCaptureDeviceInfo> arbitrarily_empty_results;
          std::move(callback).Run(arbitrarily_empty_results);
        }));

    ON_CALL(mock_source_provider_, DoGetVideoSource(_, _))
        .WillByDefault(
            Invoke([this](const std::string& device_id,
                          video_capture::mojom::VideoSourceRequest* request) {
              source_bindings_.AddBinding(&mock_source_, std::move(*request));
            }));

    ON_CALL(mock_source_, DoCreatePushSubscription(_, _, _, _, _))
        .WillByDefault(Invoke(
            [this](video_capture::mojom::ReceiverPtr& subscriber,
                   const media::VideoCaptureParams& requested_settings,
                   bool force_reopen_with_new_settings,
                   video_capture::mojom::PushVideoStreamSubscriptionRequest&
                       subscription,
                   video_capture::mojom::VideoSource::
                       CreatePushSubscriptionCallback& callback) {
              subscription_bindings_.AddBinding(&mock_subscription_,
                                                std::move(subscription));
              std::move(callback).Run(
                  video_capture::mojom::CreatePushSubscriptionResultCode::
                      kCreatedWithRequestedSettings,
                  requested_settings);
            }));
  }

  void TearDown() override {}

  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  video_capture::MockVideoCaptureService mock_video_capture_service_;
  video_capture::MockVideoSourceProvider mock_source_provider_;
  mojo::Binding<video_capture::mojom::VideoSourceProvider>
      source_provider_binding_;
  video_capture::MockVideoSource mock_source_;
  mojo::BindingSet<video_capture::mojom::VideoSource> source_bindings_;
  video_capture::MockPushSubcription mock_subscription_;
  mojo::BindingSet<video_capture::mojom::PushVideoStreamSubscription>
      subscription_bindings_;
  std::unique_ptr<ServiceVideoCaptureProvider> provider_;
  base::MockCallback<VideoCaptureProvider::GetDeviceInfosCallback> results_cb_;
  base::MockCallback<
      video_capture::mojom::VideoSourceProvider::GetSourceInfosCallback>
      service_cb_;
  base::RunLoop wait_for_connection_to_service_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceVideoCaptureProviderTest);
};

// Tests that if connection to the service is lost during an outstanding call
// to GetDeviceInfos(), the callback passed into GetDeviceInfos() still gets
// invoked.
TEST_F(ServiceVideoCaptureProviderTest,
       GetDeviceInfosAsyncInvokesCallbackWhenLosingConnection) {
  base::RunLoop run_loop;

  video_capture::mojom::VideoSourceProvider::GetSourceInfosCallback
      callback_to_be_called_by_service;
  base::RunLoop wait_for_call_to_arrive_at_service;
  EXPECT_CALL(mock_source_provider_, DoGetSourceInfos(_))
      .WillOnce(Invoke(
          [&callback_to_be_called_by_service,
           &wait_for_call_to_arrive_at_service](
              video_capture::mojom::VideoSourceProvider::GetSourceInfosCallback&
                  callback) {
            // Hold on to the callback so we can drop it later.
            callback_to_be_called_by_service = std::move(callback);
            wait_for_call_to_arrive_at_service.Quit();
          }));
  base::RunLoop wait_for_callback_from_service;
  EXPECT_CALL(results_cb_, Run(_))
      .WillOnce(Invoke(
          [&wait_for_callback_from_service](
              const std::vector<media::VideoCaptureDeviceInfo>& results) {
            EXPECT_EQ(0u, results.size());
            wait_for_callback_from_service.Quit();
          }));

  // Exercise
  provider_->GetDeviceInfosAsync(results_cb_.Get());
  wait_for_call_to_arrive_at_service.Run();

  // Simulate that the service goes down by cutting the connections.
  source_provider_binding_.Close();
  wait_for_callback_from_service.Run();
}

// Tests that |ServiceVideoCaptureProvider| closes the connection to the service
// after successfully processing a single request to GetDeviceInfos().
TEST_F(ServiceVideoCaptureProviderTest,
       ClosesServiceConnectionAfterGetDeviceInfos) {
  // Setup part 1
  video_capture::mojom::VideoSourceProvider::GetSourceInfosCallback
      callback_to_be_called_by_service;
  base::RunLoop wait_for_call_to_arrive_at_service;
  EXPECT_CALL(mock_source_provider_, DoGetSourceInfos(_))
      .WillOnce(Invoke(
          [&callback_to_be_called_by_service,
           &wait_for_call_to_arrive_at_service](
              video_capture::mojom::VideoSourceProvider::GetSourceInfosCallback&
                  callback) {
            // Hold on to the callback so we can drop it later.
            callback_to_be_called_by_service = std::move(callback);
            wait_for_call_to_arrive_at_service.Quit();
          }));

  // Exercise part 1: Make request to the service
  provider_->GetDeviceInfosAsync(results_cb_.Get());
  wait_for_call_to_arrive_at_service.Run();

  // Setup part 2: Now that the connection to the service is established, we can
  // listen for disconnects.
  base::RunLoop wait_for_connection_to_source_provider_to_close;
  source_provider_binding_.set_connection_error_handler(
      base::BindOnce([](base::RunLoop* run_loop) { run_loop->Quit(); },
                     &wait_for_connection_to_source_provider_to_close));

  // Exercise part 2: The service responds
  std::vector<media::VideoCaptureDeviceInfo> arbitrarily_empty_results;
  std::move(callback_to_be_called_by_service).Run(arbitrarily_empty_results);

  // Verification: Expect |provider_| to close the connection to the service.
  wait_for_connection_to_source_provider_to_close.Run();
}

// Tests that |ServiceVideoCaptureProvider| does not close the connection to the
// service while at least one previously handed out VideoCaptureDeviceLauncher
// instance is still using it. Then confirms that it closes the connection as
// soon as the last VideoCaptureDeviceLauncher instance is released.
TEST_F(ServiceVideoCaptureProviderTest,
       KeepsServiceConnectionWhileDeviceLauncherAlive) {
  MockVideoCaptureDeviceLauncherCallbacks mock_callbacks;

  // Exercise part 1: Create a device launcher and hold on to it.
  auto device_launcher_1 = provider_->CreateDeviceLauncher();
  base::RunLoop wait_for_launch_1;
  device_launcher_1->LaunchDeviceAsync(
      kStubDeviceId, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      kArbitraryParams, kNullReceiver, base::DoNothing(), &mock_callbacks,
      wait_for_launch_1.QuitClosure());
  wait_for_connection_to_service_.Run();
  wait_for_launch_1.Run();

  // Monitor if connection gets closed
  bool connection_has_been_closed = false;
  source_provider_binding_.set_connection_error_handler(base::BindOnce(
      [](bool* connection_has_been_closed) {
        *connection_has_been_closed = true;
      },
      &connection_has_been_closed));

  // Exercise part 2: Make a few GetDeviceInfosAsync requests
  base::RunLoop wait_for_get_device_infos_response_1;
  base::RunLoop wait_for_get_device_infos_response_2;
  provider_->GetDeviceInfosAsync(base::BindRepeating(
      [](base::RunLoop* run_loop,
         const std::vector<media::VideoCaptureDeviceInfo>&) {
        run_loop->Quit();
      },
      &wait_for_get_device_infos_response_1));
  provider_->GetDeviceInfosAsync(base::BindRepeating(
      [](base::RunLoop* run_loop,
         const std::vector<media::VideoCaptureDeviceInfo>&) {
        run_loop->Quit();
      },
      &wait_for_get_device_infos_response_2));
  wait_for_get_device_infos_response_1.Run();
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_FALSE(connection_has_been_closed);
  wait_for_get_device_infos_response_2.Run();
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_FALSE(connection_has_been_closed);

  // Exercise part 3: Create and release another device launcher
  auto device_launcher_2 = provider_->CreateDeviceLauncher();
  base::RunLoop wait_for_launch_2;
  device_launcher_2->LaunchDeviceAsync(
      kStubDeviceId, blink::mojom::MediaStreamType::DEVICE_VIDEO_CAPTURE,
      kArbitraryParams, kNullReceiver, base::DoNothing(), &mock_callbacks,
      wait_for_launch_2.QuitClosure());
  wait_for_launch_2.Run();
  device_launcher_2.reset();
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_FALSE(connection_has_been_closed);

  // Exercise part 4: Release the initial device launcher.
  device_launcher_1.reset();
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_TRUE(connection_has_been_closed);
}

// Tests that |ServiceVideoCaptureProvider| does not close the connection to the
// service while at least one answer to a GetDeviceInfos() request is still
// pending. Confirms that it closes the connection as soon as the last pending
// request is answered.
TEST_F(ServiceVideoCaptureProviderTest,
       DoesNotCloseServiceConnectionWhileGetDeviceInfoResponsePending) {
  // When GetDeviceInfos gets called, hold on to the callbacks, but do not
  // yet invoke them.
  std::vector<video_capture::mojom::VideoSourceProvider::GetSourceInfosCallback>
      callbacks_to_be_called_by_service;
  ON_CALL(mock_source_provider_, DoGetSourceInfos(_))
      .WillByDefault(Invoke(
          [&callbacks_to_be_called_by_service](
              video_capture::mojom::VideoSourceProvider::GetSourceInfosCallback&
                  callback) {
            callbacks_to_be_called_by_service.push_back(std::move(callback));
          }));

  // Make initial call to GetDeviceInfosAsync(). The service does not yet
  // respond.
  provider_->GetDeviceInfosAsync(base::BindRepeating(
      [](const std::vector<media::VideoCaptureDeviceInfo>&) {}));
  // Make an additional call to GetDeviceInfosAsync().
  provider_->GetDeviceInfosAsync(base::BindRepeating(
      [](const std::vector<media::VideoCaptureDeviceInfo>&) {}));
  {
    base::RunLoop give_mojo_chance_to_process;
    give_mojo_chance_to_process.RunUntilIdle();
  }
  ASSERT_EQ(2u, callbacks_to_be_called_by_service.size());

  // Monitor if connection gets closed
  bool connection_has_been_closed = false;
  source_provider_binding_.set_connection_error_handler(base::BindOnce(
      [](bool* connection_has_been_closed) {
        *connection_has_been_closed = true;
      },
      &connection_has_been_closed));

  // The service now responds to the first request.
  std::vector<media::VideoCaptureDeviceInfo> arbitrarily_empty_results;
  std::move(callbacks_to_be_called_by_service[0])
      .Run(arbitrarily_empty_results);
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_FALSE(connection_has_been_closed);

  // Create and release a device launcher
  auto device_launcher = provider_->CreateDeviceLauncher();
  device_launcher.reset();
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_FALSE(connection_has_been_closed);

  // The service now responds to the second request.
  std::move(callbacks_to_be_called_by_service[1])
      .Run(arbitrarily_empty_results);
  {
    base::RunLoop give_provider_chance_to_disconnect;
    give_provider_chance_to_disconnect.RunUntilIdle();
  }
  ASSERT_TRUE(connection_has_been_closed);
}

}  // namespace content
