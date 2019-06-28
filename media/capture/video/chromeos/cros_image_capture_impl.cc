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

void CrosImageCaptureImpl::OnGotCameraInfo(
    GetCameraInfoCallback callback,
    cros::mojom::CameraInfoPtr camera_info) {
  std::move(callback).Run(std::move(camera_info));
}

}  // namespace media
