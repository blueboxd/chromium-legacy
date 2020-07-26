// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "media/capture/video/mac/video_capture_device_factory_mac.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "media/capture/video/mac/video_capture_device_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Video capture code on MacOSX must run on a CFRunLoop enabled thread
// for interaction with AVFoundation.
// In order to make the test case run on the actual message loop that has
// been created for this thread, we need to run it inside a RunLoop. This is
// required, because on MacOS the capture code must run on a CFRunLoop
// enabled message loop.
void RunTestCase(base::OnceClosure test_case) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::UI);
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::RunLoop* run_loop, base::OnceClosure* test_case) {
                       std::move(*test_case).Run();
                       run_loop->Quit();
                     },
                     &run_loop, &test_case));
  run_loop.Run();
}

void GetDevicesInfo(VideoCaptureDeviceFactoryMac* video_capture_device_factory,
                    std::vector<VideoCaptureDeviceInfo>* descriptors) {
  base::RunLoop run_loop;
  video_capture_device_factory->GetDevicesInfo(base::BindLambdaForTesting(
      [descriptors, &run_loop](std::vector<VideoCaptureDeviceInfo> result) {
        *descriptors = std::move(result);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST(VideoCaptureDeviceFactoryMacTest, ListDevicesAVFoundation) {
  RunTestCase(base::BindOnce([]() {
    VideoCaptureDeviceFactoryMac video_capture_device_factory;

    std::vector<VideoCaptureDeviceInfo> devices_info;
    GetDevicesInfo(&video_capture_device_factory, &devices_info);
    if (devices_info.empty()) {
      DVLOG(1) << "No camera available. Exiting test.";
      return;
    }
    for (const auto& device : devices_info) {
      EXPECT_EQ(VideoCaptureApi::MACOSX_AVFOUNDATION,
                device.descriptor.capture_api);
    }
  }));
}

TEST(VideoCaptureDeviceFactoryMacTest, ListDevicesWithNoPanTiltZoomSupport) {
  RunTestCase(base::BindOnce([]() {
    VideoCaptureDeviceFactoryMac video_capture_device_factory;

    std::vector<VideoCaptureDeviceInfo> devices_info;
    GetDevicesInfo(&video_capture_device_factory, &devices_info);
    if (devices_info.empty()) {
      DVLOG(1) << "No camera available. Exiting test.";
      return;
    }
    for (const auto& device : devices_info)
      EXPECT_FALSE(device.descriptor.pan_tilt_zoom_supported());
  }));
}

}  // namespace media
