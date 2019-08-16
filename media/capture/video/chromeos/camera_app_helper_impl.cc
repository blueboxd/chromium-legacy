// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/video/chromeos/camera_app_helper_impl.h"

namespace media {

CameraAppHelperImpl::CameraAppHelperImpl(IntentCallback intent_callback)
    : intent_callback_(std::move(intent_callback)) {}

CameraAppHelperImpl::~CameraAppHelperImpl() = default;

void CameraAppHelperImpl::OnIntentHandled(
    uint32_t intent_id,
    bool is_success,
    const std::vector<uint8_t>& captured_data) {
  intent_callback_.Run(intent_id, is_success, captured_data);
}

}  // namespace media