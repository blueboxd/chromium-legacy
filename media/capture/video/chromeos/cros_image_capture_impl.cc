// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/cros_image_capture_impl.h"

#include <string>
#include <utility>
#include <vector>

#include "base/task/post_task.h"
#include "media/base/bind_to_current_loop.h"

namespace media {

CrosImageCaptureImpl::CrosImageCaptureImpl(ReprocessManager* reprocess_manager)
    : reprocess_manager_(reprocess_manager) {}

CrosImageCaptureImpl::~CrosImageCaptureImpl() = default;

void CrosImageCaptureImpl::GetCameraInfo(const std::string& device_id,
                                         GetCameraInfoCallback callback) {
  reprocess_manager_->GetCameraInfo(
      device_id, media::BindToCurrentLoop(base::BindOnce(
                     &CrosImageCaptureImpl::OnGotCameraInfo,
                     base::Unretained(this), std::move(callback))));
}

void CrosImageCaptureImpl::SetReprocessOption(
    const std::string& device_id,
    cros::mojom::Effect effect,
    SetReprocessOptionCallback callback) {
  reprocess_manager_->SetReprocessOption(
      device_id, effect, media::BindToCurrentLoop(std::move(callback)));
}

void CrosImageCaptureImpl::SetFpsRange(const std::string& device_id,
                                       const uint32_t stream_width,
                                       const uint32_t stream_height,
                                       const int32_t min_fps,
                                       const int32_t max_fps,
                                       SetFpsRangeCallback callback) {
  reprocess_manager_->SetFpsRange(
      device_id, stream_width, stream_height, min_fps, max_fps,
      media::BindToCurrentLoop(std::move(callback)));
}

void CrosImageCaptureImpl::OnGotCameraInfo(
    GetCameraInfoCallback callback,
    cros::mojom::CameraInfoPtr camera_info) {
  std::move(callback).Run(std::move(camera_info));
}

void CrosImageCaptureImpl::OnIntentHandled(
    uint32_t intent_id,
    bool is_success,
    const std::vector<uint8_t>& captured_data) {
  NOTREACHED() << "Should be handled in RendererFacingCrosImageCapture";
}

}  // namespace media
