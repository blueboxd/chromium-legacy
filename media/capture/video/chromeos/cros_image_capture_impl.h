// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAPTURE_VIDEO_CHROMEOS_CROS_IMAGE_CAPTURE_IMPL_H_
#define MEDIA_CAPTURE_VIDEO_CHROMEOS_CROS_IMAGE_CAPTURE_IMPL_H_

#include <string>

#include "media/capture/video/chromeos/mojo/camera_common.mojom.h"
#include "media/capture/video/chromeos/mojo/cros_image_capture.mojom.h"
#include "media/capture/video/chromeos/reprocess_manager.h"
#include "mojo/public/cpp/bindings/binding_set.h"

namespace media {

class CrosImageCaptureImpl : public cros::mojom::CrosImageCapture {
 public:
  explicit CrosImageCaptureImpl(ReprocessManager* reprocess_manager);

  ~CrosImageCaptureImpl() override;

  void BindRequest(cros::mojom::CrosImageCaptureRequest request);

  // cros::mojom::CrosImageCapture implementations.

  void GetCameraInfo(const std::string& device_id,
                     GetCameraInfoCallback callback) override;

  void SetReprocessOption(const std::string& device_id,
                          cros::mojom::Effect effect,
                          SetReprocessOptionCallback callback) override;

  void SetFpsRange(const std::string& device_id,
                   const uint32_t stream_width,
                   const uint32_t stream_height,
                   const int32_t min_fps,
                   const int32_t max_fps,
                   SetFpsRangeCallback callback) override;

  void OnIntentHandled(uint32_t intent_id,
                       bool is_success,
                       const std::vector<uint8_t>& captured_data) override;

 private:
  void OnGotCameraInfo(GetCameraInfoCallback callback,
                       cros::mojom::CameraInfoPtr camera_info);

  ReprocessManager* reprocess_manager_;  // weak

  DISALLOW_COPY_AND_ASSIGN(CrosImageCaptureImpl);
};

}  // namespace media

#endif  // MEDIA_CAPTURE_VIDEO_CHROMEOS_CROS_IMAGE_CAPTURE_IMPL_H_
