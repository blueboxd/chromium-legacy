// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_
#define SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_

#include "services/video_capture/public/mojom/device_factory.mojom.h"

#if defined(OS_CHROMEOS)
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#endif  // defined(OS_CHROMEOS)

namespace video_capture {

class DeviceFactory : public mojom::DeviceFactory {
 public:
#if defined(OS_CHROMEOS)
  virtual void BindCrosImageCaptureRequest(
      cros::mojom::CrosImageCaptureRequest request) = 0;
#endif  // defined(OS_CHROMEOS)
};

}  // namespace video_capture

#endif  // SERVICES_VIDEO_CAPTURE_DEVICE_FACTORY_H_
