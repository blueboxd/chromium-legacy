// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_H_
#define MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_H_

#include "media/capture/video/video_capture_device_factory.h"
#include "media/capture/video/video_capture_device_info.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/mojom/cros_image_capture.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace media {

// GetDeviceInfosAsync() should be called at least once before calling
// CreateDevice(), because otherwise CreateDevice() will allways return nullptr.
class CAPTURE_EXPORT VideoCaptureSystem {
 public:
  using DeviceInfoCallback =
      base::OnceCallback<void(const std::vector<VideoCaptureDeviceInfo>&)>;

  virtual ~VideoCaptureSystem() {}

  // The passed-in |result_callback| must have ownership of the called
  // VideoCaptureSystem instance to guarantee that it stays alive during the
  // asynchronous operation. |result_callback| is invoked on the same thread
  // that calls GetDeviceInfosAsync()
  virtual void GetDeviceInfosAsync(DeviceInfoCallback result_callback) = 0;

  // Creates a VideoCaptureDevice object. Returns nullptr if something goes
  // wrong.
  virtual std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const std::string& device_id) = 0;

#if defined(OS_CHROMEOS)
  // Pass the mojo request to bind with DeviceFactory for Chrome OS.
  virtual void BindCrosImageCaptureRequest(
      cros::mojom::CrosImageCaptureRequest request) = 0;
#endif  // defined(OS_CHROMEOS)
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_VIDEO_CAPTURE_SYSTEM_H_
