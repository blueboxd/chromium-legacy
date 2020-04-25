// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FACTORY_FUCHSIA_H_
#define MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FACTORY_FUCHSIA_H_

#include <fuchsia/camera3/cpp/fidl.h>

#include <map>

#include "base/containers/small_map.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "media/capture/video/video_capture_device_factory.h"

namespace media {

class CAPTURE_EXPORT VideoCaptureDeviceFactoryFuchsia
    : public VideoCaptureDeviceFactory {
 public:
  VideoCaptureDeviceFactoryFuchsia();
  ~VideoCaptureDeviceFactoryFuchsia() override;

  VideoCaptureDeviceFactoryFuchsia(const VideoCaptureDeviceFactoryFuchsia&) =
      delete;
  VideoCaptureDeviceFactoryFuchsia& operator=(
      const VideoCaptureDeviceFactoryFuchsia&) = delete;

  // VideoCaptureDeviceFactory implementation.
  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(const VideoCaptureDeviceDescriptor& device,
                           VideoCaptureFormats* supported_formats) override;

 private:
  // Helper class used to fetch per-device information.
  class DeviceInfoFetcher;

  void Initialize();

  void OnDeviceWatcherDisconnected(zx_status_t status);

  void WatchDevices();
  void OnWatchDevicesResult(
      std::vector<fuchsia::camera3::WatchDevicesEvent> events);

  fuchsia::camera3::DeviceWatcherPtr device_watcher_;
  base::small_map<std::map<uint64_t, std::unique_ptr<DeviceInfoFetcher>>>
      devices_;

  // RunLoop used to wait for the first WatchDevices() response. Currently
  // required because GetDeviceDescriptors() is synchronous.
  // TODO(crbug.com/1072932) Refactor interface to allow asynchronous
  // enumeration and remove this hack.
  base::Optional<base::RunLoop> first_update_run_loop_;
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_FUCHSIA_VIDEO_CAPTURE_DEVICE_FACTORY_FUCHSIA_H_
