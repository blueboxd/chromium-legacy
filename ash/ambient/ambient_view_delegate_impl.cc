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
  // Exit ambient mode by closing the widget when user interacts with the
  // background photo using mouse or gestures. We do this asynchronously to
  // ensure that for a mouse moved event, the widget will be destroyed *after*
  // its cursor has been updated in |RootView::OnMouseMoved|.
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](const base::WeakPtr<AmbientViewDelegateImpl>& weak_ptr) {
            if (weak_ptr)
              weak_ptr->ambient_controller_->Stop();
          },
          weak_factory_.GetWeakPtr()));
}

}  // namespace ash
