// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/camera_app_helper_impl.h"

#include "ash/public/cpp/tablet_mode.h"

namespace chromeos_camera {

CameraAppHelperImpl::CameraAppHelperImpl(IntentCallback intent_callback)
    : intent_callback_(std::move(intent_callback)) {}

CameraAppHelperImpl::~CameraAppHelperImpl() = default;

void CameraAppHelperImpl::OnIntentHandled(
    uint32_t intent_id,
    bool is_success,
    const std::vector<uint8_t>& captured_data) {
  intent_callback_.Run(intent_id, is_success, captured_data);
}

void CameraAppHelperImpl::IsTabletMode(IsTabletModeCallback callback) {
  std::move(callback).Run(ash::TabletMode::Get()->InTabletMode());
}

}  // namespace chromeos_camera
