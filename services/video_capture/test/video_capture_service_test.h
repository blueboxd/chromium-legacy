// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_TEST_VIDEO_CAPTURE_SERVICE_TEST_H_
#define SERVICES_VIDEO_CAPTURE_TEST_VIDEO_CAPTURE_SERVICE_TEST_H_

#include "base/macros.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/video_capture/public/mojom/device_factory.mojom.h"
#include "services/video_capture/public/mojom/video_capture_service.mojom.h"
#include "services/video_capture/public/mojom/virtual_device.mojom.h"
#include "services/video_capture/video_capture_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace video_capture {

class MockProducer;

// Basic test fixture that sets up a connection to the fake device factory.
class VideoCaptureServiceTest : public testing::Test {
 public:
  VideoCaptureServiceTest();
  ~VideoCaptureServiceTest() override;

  void SetUp() override;

 protected:
  struct SharedMemoryVirtualDeviceContext {
    SharedMemoryVirtualDeviceContext(mojom::ProducerRequest producer_request);
    ~SharedMemoryVirtualDeviceContext();

    std::unique_ptr<MockProducer> mock_producer;
    mojom::SharedMemoryVirtualDevicePtr device;
  };

  std::unique_ptr<SharedMemoryVirtualDeviceContext>
  AddSharedMemoryVirtualDevice(const std::string& device_id);

  mojom::TextureVirtualDevicePtr AddTextureVirtualDevice(
      const std::string& device_id);

  base::test::ScopedTaskEnvironment task_environment_;

  std::unique_ptr<VideoCaptureServiceImpl> service_impl_;
  mojo::Remote<mojom::VideoCaptureService> service_remote_;
  mojom::DeviceFactoryPtr factory_;
  base::MockCallback<mojom::DeviceFactory::GetDeviceInfosCallback>
      device_info_receiver_;

  DISALLOW_COPY_AND_ASSIGN(VideoCaptureServiceTest);
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_TEST_VIDEO_CAPTURE_SERVICE_TEST_H_
