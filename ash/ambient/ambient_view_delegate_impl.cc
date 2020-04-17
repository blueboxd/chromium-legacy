// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ambient_view_delegate_impl.h"

#include "ash/ambient/ambient_controller.h"
#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"

namespace ash {

AmbientViewDelegateImpl::AmbientViewDelegateImpl(
    AmbientController* ambient_controller)
    : ambient_controller_(ambient_controller) {}

AmbientViewDelegateImpl::~AmbientViewDelegateImpl() = default;

PhotoModel* AmbientViewDelegateImpl::GetPhotoModel() {
  return ambient_controller_->photo_model();
}

void AmbientViewDelegateImpl::OnBackgroundPhotoEvents() {
  ambient_controller_->OnBackgroundPhotoEvents();
}

}  // namespace ash
