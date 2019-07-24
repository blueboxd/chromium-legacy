// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/renderer_facing_cros_image_capture.h"

#include <errno.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/task/post_task.h"
#include "media/base/bind_to_current_loop.h"
#include "media/capture/mojom/image_capture.mojom.h"
#include "media/capture/video/chromeos/mojo/camera_common.mojom.h"

namespace media {

RendererFacingCrosImageCapture::RendererFacingCrosImageCapture(
    cros::mojom::CrosImageCapturePtr api_ptr,
    DeviceIdMappingCallback mapping_callback,
    IntentCallback intent_callback)
    : cros_image_capture_(std::move(api_ptr)),
      mapping_callback_(std::move(mapping_callback)),
      intent_callback_(std::move(intent_callback)),
      weak_ptr_factory_(this) {}

RendererFacingCrosImageCapture::~RendererFacingCrosImageCapture() = default;

void RendererFacingCrosImageCapture::GetCameraInfoWithRealId(
    GetCameraInfoCallback callback,
    const base::Optional<std::string>& device_id) {
  if (!device_id.has_value()) {
    std::move(callback).Run({});
    return;
  }
  cros_image_capture_->GetCameraInfo(*device_id, std::move(callback));
}

void RendererFacingCrosImageCapture::SetReprocessOptionWithRealId(
    cros::mojom::Effect effect,
    SetReprocessOptionCallback callback,
    const base::Optional<std::string>& device_id) {
  if (!device_id.has_value()) {
    std::move(callback).Run(-EINVAL, {});
    return;
  }
  cros_image_capture_->SetReprocessOption(*device_id, effect,
                                          std::move(callback));
}

void RendererFacingCrosImageCapture::SetFpsRangeWithRealId(
    const uint32_t stream_width,
    const uint32_t stream_height,
    const int32_t min_frame_rate,
    const int32_t max_frame_rate,
    SetFpsRangeCallback callback,
    const base::Optional<std::string>& device_id) {
  if (!device_id.has_value()) {
    std::move(callback).Run(false);
    return;
  }
  cros_image_capture_->SetFpsRange(*device_id, stream_width, stream_height,
                                   min_frame_rate, max_frame_rate,
                                   std::move(callback));
}

void RendererFacingCrosImageCapture::GetCameraInfo(
    const std::string& source_id,
    GetCameraInfoCallback callback) {
  mapping_callback_.Run(
      source_id, media::BindToCurrentLoop(base::BindOnce(
                     &RendererFacingCrosImageCapture::GetCameraInfoWithRealId,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void RendererFacingCrosImageCapture::SetReprocessOption(
    const std::string& source_id,
    cros::mojom::Effect effect,
    SetReprocessOptionCallback callback) {
  mapping_callback_.Run(
      source_id,
      media::BindToCurrentLoop(base::BindOnce(
          &RendererFacingCrosImageCapture::SetReprocessOptionWithRealId,
          weak_ptr_factory_.GetWeakPtr(), effect, std::move(callback))));
}

void RendererFacingCrosImageCapture::SetFpsRange(const std::string& source_id,
                                                 const uint32_t stream_width,
                                                 const uint32_t stream_height,
                                                 const int32_t min_frame_rate,
                                                 const int32_t max_frame_rate,
                                                 SetFpsRangeCallback callback) {
  mapping_callback_.Run(
      source_id,
      media::BindToCurrentLoop(base::BindOnce(
          &RendererFacingCrosImageCapture::SetFpsRangeWithRealId,
          weak_ptr_factory_.GetWeakPtr(), stream_width, stream_height,
          min_frame_rate, max_frame_rate, std::move(callback))));
}

void RendererFacingCrosImageCapture::OnIntentHandled(
    uint32_t intent_id,
    bool is_success,
    const std::vector<uint8_t>& captured_data) {
  intent_callback_.Run(intent_id, is_success, captured_data);
}

}  // namespace media