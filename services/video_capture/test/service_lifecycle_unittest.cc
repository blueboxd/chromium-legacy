// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "base/timer/timer.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/cpp/mock_receiver.h"
#include "services/video_capture/public/mojom/constants.mojom.h"
#include "services/video_capture/public/mojom/device.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/mojom/video_source_provider.mojom.h"
#include "services/video_capture/video_capture_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_capture {

using testing::_;
using testing::Exactly;
using testing::Invoke;
using testing::InvokeWithoutArgs;

// Test fixture that creates a video_capture::ServiceImpl and sets up a
// local service_manager::Connector through which client code can connect to
// it.
class VideoCaptureServiceLifecycleTest : public ::testing::Test {
 public:
  VideoCaptureServiceLifecycleTest() = default;
  ~VideoCaptureServiceLifecycleTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeDeviceForMediaStream);
    service_impl_ = std::make_unique<VideoCaptureServiceImpl>(
        service_remote_.BindNewPipeAndPassReceiver(),
        scoped_task_environment_.GetMainThreadTaskRunner());
    service_remote_.set_idle_handler(
        base::TimeDelta(),
        base::BindRepeating(&VideoCaptureServiceLifecycleTest::OnServiceIdle,
                            base::Unretained(this)));
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  std::unique_ptr<VideoCaptureServiceImpl> service_impl_;
  mojo::Remote<mojom::VideoCaptureService> service_remote_;
  base::MockCallback<mojom::DeviceFactory::GetDeviceInfosCallback>
      device_info_receiver_;
  base::RunLoop service_idle_wait_loop_;

 private:
  void OnServiceIdle() { service_idle_wait_loop_.Quit(); }

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureServiceLifecycleTest);
};

// Tests that the service quits when the only client disconnects after not
// having done anything other than obtaining a connection to the device factory.
TEST_F(VideoCaptureServiceLifecycleTest,
       ServiceQuitsWhenSingleDeviceFactoryClientDisconnected) {
  mojom::DeviceFactoryPtr factory;
  service_remote_->ConnectToDeviceFactory(mojo::MakeRequest(&factory));
  factory.reset();
  service_idle_wait_loop_.Run();
}

// Tests that the service quits when the only client disconnects after not
// having done anything other than obtaining a connection to the video source
// provider.
TEST_F(VideoCaptureServiceLifecycleTest,
       ServiceQuitsWhenSingleVideoSourceProviderClientDisconnected) {
  mojom::VideoSourceProviderPtr source_provider;
  service_remote_->ConnectToVideoSourceProvider(
      mojo::MakeRequest(&source_provider));
  source_provider.reset();
  service_idle_wait_loop_.Run();
}

// Tests that the service quits when the only client disconnects after
// enumerating devices via the video source provider.
TEST_F(VideoCaptureServiceLifecycleTest, ServiceQuitsAfterEnumeratingDevices) {
  mojom::VideoSourceProviderPtr source_provider;
  service_remote_->ConnectToVideoSourceProvider(
      mojo::MakeRequest(&source_provider));

  base::RunLoop wait_loop;
  EXPECT_CALL(device_info_receiver_, Run(_))
      .WillOnce(
          Invoke([&wait_loop](
                     const std::vector<media::VideoCaptureDeviceInfo>& infos) {
            wait_loop.Quit();
          }));
  source_provider->GetSourceInfos(device_info_receiver_.Get());
  wait_loop.Run();

  source_provider.reset();

  service_idle_wait_loop_.Run();
}

// Tests that enumerating devices works after the only client disconnects and
// reconnects via the video source provider.
TEST_F(VideoCaptureServiceLifecycleTest, EnumerateDevicesAfterReconnect) {
  // Connect |source_provider|.
  mojom::VideoSourceProviderPtr source_provider;
  service_remote_->ConnectToVideoSourceProvider(
      mojo::MakeRequest(&source_provider));

  // Disconnect |source_provider| and wait for the disconnect to propagate to
  // the service.
  {
    base::RunLoop wait_loop;
    source_provider->Close(base::BindOnce(
        [](base::RunLoop* wait_loop) { wait_loop->Quit(); }, &wait_loop));
    wait_loop.Run();
    source_provider.reset();
  }

  // Reconnect |source_provider|.
  service_remote_->ConnectToVideoSourceProvider(
      mojo::MakeRequest(&source_provider));

  // Enumerate devices.
  base::RunLoop wait_loop;
  EXPECT_CALL(device_info_receiver_, Run(_))
      .WillOnce(
          Invoke([&wait_loop](
                     const std::vector<media::VideoCaptureDeviceInfo>& infos) {
            wait_loop.Quit();
          }));
  source_provider->GetSourceInfos(device_info_receiver_.Get());
  wait_loop.Run();

  source_provider.reset();

  service_idle_wait_loop_.Run();
}

// Tests that the service quits when the last client disconnects while using a
// device.
TEST_F(VideoCaptureServiceLifecycleTest,
       ServiceQuitsWhenClientDisconnectsWhileUsingDevice) {
  mojom::DeviceFactoryPtr factory;
  service_remote_->ConnectToDeviceFactory(mojo::MakeRequest(&factory));

  // Connect to and start first device (in this case a fake camera).
  media::VideoCaptureDeviceInfo fake_device_info;
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(device_info_receiver_, Run(_))
        .WillOnce(Invoke(
            [&fake_device_info, &wait_loop](
                const std::vector<media::VideoCaptureDeviceInfo>& infos) {
              fake_device_info = infos[0];
              wait_loop.Quit();
            }));
    factory->GetDeviceInfos(device_info_receiver_.Get());
    wait_loop.Run();
  }
  mojom::DevicePtr fake_device;
  factory->CreateDevice(
      std::move(fake_device_info.descriptor.device_id),
      mojo::MakeRequest(&fake_device),
      base::BindOnce([](mojom::DeviceAccessResultCode result_code) {
        ASSERT_EQ(mojom::DeviceAccessResultCode::SUCCESS, result_code);
      }));
  media::VideoCaptureParams requestable_settings;
  requestable_settings.requested_format = fake_device_info.supported_formats[0];
  mojom::ReceiverPtr receiver_proxy;
  MockReceiver mock_receiver(mojo::MakeRequest(&receiver_proxy));
  fake_device->Start(requestable_settings, std::move(receiver_proxy));
  {
    base::RunLoop wait_loop;
    EXPECT_CALL(mock_receiver, OnStarted()).WillOnce([&wait_loop]() {
      wait_loop.Quit();
    });
    wait_loop.Run();
  }

  // Disconnect
  fake_device.reset();
  factory.reset();

  service_idle_wait_loop_.Run();
}

}  // namespace video_capture
