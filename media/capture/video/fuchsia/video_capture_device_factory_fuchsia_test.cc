// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/fuchsia/video_capture_device_factory_fuchsia.h"

#include "base/fuchsia/test_component_context_for_process.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "media/fuchsia/camera/fake_fuchsia_camera.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class VideoCaptureDeviceFactoryFuchsiaTest : public testing::Test {
 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
  base::TestComponentContextForProcess test_context_;

  FakeCameraDeviceWatcher fake_device_watcher_{
      test_context_.additional_services()};

  VideoCaptureDeviceFactoryFuchsia device_factory_;
};

TEST_F(VideoCaptureDeviceFactoryFuchsiaTest, EnumerateDevices) {
  media::VideoCaptureDeviceDescriptors device_descriptors;
  device_factory_.GetDeviceDescriptors(&device_descriptors);

  EXPECT_EQ(device_descriptors.size(), 1U);
}

TEST_F(VideoCaptureDeviceFactoryFuchsiaTest, EnumerateDevicesAfterDisconnect) {
  media::VideoCaptureDeviceDescriptors device_descriptors;
  device_factory_.GetDeviceDescriptors(&device_descriptors);

  EXPECT_EQ(device_descriptors.size(), 1U);

  device_descriptors.clear();

  // Disconnect DeviceWatcher and run the run loop so |device_factory_| can
  // handle the disconnect.
  fake_device_watcher_.DisconnectClients();
  base::RunLoop().RunUntilIdle();

  // Try enumerating devices again. DeviceWatcher is expected to be reconnected.
  device_factory_.GetDeviceDescriptors(&device_descriptors);

  EXPECT_EQ(device_descriptors.size(), 1U);
}

}  // namespace media
