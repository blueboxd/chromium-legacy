// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/photo_view.h"

#include <algorithm>
#include <memory>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/photo_model.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

// AmbientBackgroundImageView--------------------------------------------------
// A custom ImageView for ambient mode to handle specific mouse/gesture events
// when user interacting with the background photos.
class AmbientBackgroundImageView : public views::ImageView {
 public:
  explicit AmbientBackgroundImageView(AmbientViewDelegate* delegate)
      : delegate_(delegate) {
    DCHECK(delegate_);
  }
  AmbientBackgroundImageView(const AmbientBackgroundImageView&) = delete;
  AmbientBackgroundImageView& operator=(AmbientBackgroundImageView&) = delete;
  ~AmbientBackgroundImageView() override = default;

  // views::View:
  const char* GetClassName() const override {
    return "AmbientBackgroundImageView";
  }

  bool OnMousePressed(const ui::MouseEvent& event) override {
    delegate_->OnBackgroundPhotoEvents();
    return true;
  }

  void OnMouseMoved(const ui::MouseEvent& event) override {
    delegate_->OnBackgroundPhotoEvents();
  }

  void OnGestureEvent(ui::GestureEvent* event) override {
    if (event->type() == ui::ET_GESTURE_TAP) {
      delegate_->OnBackgroundPhotoEvents();
      event->SetHandled();
    }
  }

 private:
  // Owned by |AmbientController| and should always outlive |this|.
  AmbientViewDelegate* delegate_ = nullptr;
};

// PhotoView ------------------------------------------------------------------
PhotoView::PhotoView(AmbientViewDelegate* delegate) : delegate_(delegate) {
  DCHECK(delegate_);
  Init();
}

PhotoView::~PhotoView() {
  delegate_->GetPhotoModel()->RemoveObserver(this);
}

const char* PhotoView::GetClassName() const {
  return "PhotoView";
}

void PhotoView::AddedToWidget() {
  // Set the bounds to show |image_view_curr_| for the first time.
  // TODO(b/140066694): Handle display configuration changes, e.g. resolution,
  // rotation, etc.
  const gfx::Size widget_size = GetWidget()->GetRootView()->size();
  image_view_prev_->SetImageSize(widget_size);
  image_view_curr_->SetImageSize(widget_size);
  image_view_next_->SetImageSize(widget_size);
  gfx::Rect view_bounds = gfx::Rect(GetPreferredSize());
  const int width = widget_size.width();
  view_bounds.set_x(-width);
  SetBoundsRect(view_bounds);
}

void PhotoView::OnImagesChanged() {
  UpdateImages();
  StartSlideAnimation();
}

void PhotoView::Init() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  image_view_prev_ =
      AddChildView(std::make_unique<AmbientBackgroundImageView>(delegate_));
  image_view_curr_ =
      AddChildView(std::make_unique<AmbientBackgroundImageView>(delegate_));
  image_view_next_ =
      AddChildView(std::make_unique<AmbientBackgroundImageView>(delegate_));

  delegate_->GetPhotoModel()->AddObserver(this);
}

void PhotoView::UpdateImages() {
  // TODO(b/140193766): Investigate a more efficient way to update images and do
  // layer animation.
  auto* model = delegate_->GetPhotoModel();
  image_view_prev_->SetImage(model->GetPrevImage());
  image_view_curr_->SetImage(model->GetCurrImage());
  image_view_next_->SetImage(model->GetNextImage());
}

void PhotoView::StartSlideAnimation() {
  if (!CanAnimate())
    return;

  ui::Layer* layer = this->layer();
  const int x_offset = image_view_prev_->GetPreferredSize().width();
  gfx::Transform transform;
  transform.Translate(x_offset, 0);
  layer->SetTransform(transform);
  {
    ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
    animation.SetTransitionDuration(kAnimationDuration);
    animation.SetTweenType(gfx::Tween::EASE_OUT);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    layer->SetTransform(gfx::Transform());
  }
}

bool PhotoView::CanAnimate() const {
  // Cannot do slide animation from previous to current image.
  return !image_view_prev_->GetImage().isNull();
}

}  // namespace ash
