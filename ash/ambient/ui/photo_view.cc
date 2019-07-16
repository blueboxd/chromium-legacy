// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/photo_view.h"

#include "ash/ambient/ambient_controller.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"

namespace ash {

PhotoView::PhotoView(AmbientController* ambient_controller)
    : ambient_controller_(ambient_controller) {
  Init();
}

PhotoView::~PhotoView() {
  // |ambient_controller_| outlives this view.
  ambient_controller_->RemovePhotoModelObserver(this);
}

const char* PhotoView::GetClassName() const {
  return "PhotoView";
}

void PhotoView::OnBoundsChanged(const gfx::Rect& prev_bounds) {
  image_view_->SetBoundsRect(GetLocalBounds());
}

void PhotoView::OnImageAvailable(const gfx::ImageSkia& image) {
  // TODO(wutao): Options to choose how to layout image, such as STRETCH,
  // CENTER, CENTER_CROPPED etc.
  image_view_->SetImage(image);
}

void PhotoView::Init() {
  image_view_ = new views::ImageView();
  AddChildView(image_view_);

  // |ambient_controller_| outlives this view.
  ambient_controller_->AddPhotoModelObserver(this);
}

}  // namespace ash
