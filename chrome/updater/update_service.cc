// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_service.h"

namespace updater {

UpdateService::UpdateState::UpdateState() = default;
UpdateService::UpdateState::UpdateState(const UpdateState&) = default;
UpdateService::UpdateState& UpdateService::UpdateState::operator=(
    const UpdateState&) = default;
UpdateService::UpdateState::UpdateState(UpdateState&&) = default;
UpdateService::UpdateState& UpdateService::UpdateState::operator=(
    UpdateState&&) = default;
UpdateService::UpdateState::~UpdateState() = default;

}  // namespace updater
