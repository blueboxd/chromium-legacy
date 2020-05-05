// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/photo_view.h"

#include <algorithm>
#include <memory>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/window.h"
#include "ui/compositor/animation_metrics_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class PhotoViewMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  PhotoViewMetricsReporter() = default;
  PhotoViewMetricsReporter(const PhotoViewMetricsReporter&) = delete;
  PhotoViewMetricsReporter& operator=(const PhotoViewMetricsReporter&) = delete;
  ~PhotoViewMetricsReporter() override = default;

  void Report(int value) override {
    UMA_HISTOGRAM_PERCENTAGE(
        "Ash.AmbientMode.AnimationSmoothness.PhotoTransition", value);
  }
};

}  // namespace

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
PhotoView::PhotoView(AmbientViewDelegate* delegate)
    : delegate_(delegate),
      metrics_reporter_(std::make_unique<PhotoViewMetricsReporter>()) {
  DCHECK(delegate_);
  Init();
}

PhotoView::~PhotoView() {
  delegate_->GetAmbientBackendModel()->RemoveObserver(this);
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
  // If NeedToAnimate() is true, will start transition animation and
  // UpdateImages() when animation completes. Otherwise, update images
  // immediately.
  if (NeedToAnimateTransition()) {
    StartTransitionAnimation();
    return;
  }

  UpdateImages();
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

  delegate_->GetAmbientBackendModel()->AddObserver(this);
}

void PhotoView::UpdateImages() {
  // TODO(b/140193766): Investigate a more efficient way to update images and do
  // layer animation.
  auto* model = delegate_->GetAmbientBackendModel();
  image_view_prev_->SetImage(model->GetPrevImage());
  image_view_curr_->SetImage(model->GetCurrImage());
  image_view_next_->SetImage(model->GetNextImage());
}

void PhotoView::StartTransitionAnimation() {
  ui::Layer* layer = this->layer();
  ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
  animation.SetTransitionDuration(kAnimationDuration);
  animation.SetTweenType(gfx::Tween::FAST_OUT_LINEAR_IN);
  animation.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  animation.SetAnimationMetricsReporter(metrics_reporter_.get());
  animation.AddObserver(this);

  const int x_offset = image_view_curr_->GetPreferredSize().width();
  gfx::Transform transform;
  transform.Translate(-x_offset, 0);
  layer->SetTransform(transform);
}

void PhotoView::OnImplicitAnimationsCompleted() {
  // Layer transform and images update will be applied on the next frame at the
  // same time.
  this->layer()->SetTransform(gfx::Transform());
  UpdateImages();
}

bool PhotoView::NeedToAnimateTransition() const {
  // Can do transition animation from current to next image.
  return !image_view_next_->GetImage().isNull();
}

}  // namespace ash
