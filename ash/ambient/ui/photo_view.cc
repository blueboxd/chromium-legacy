// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/photo_view.h"

#include <algorithm>
#include <iterator>
#include <memory>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/ui/ambient_background_image_view.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/assistant/ui/assistant_view_ids.h"
#include "ash/public/cpp/metrics_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/compositor/animation_metrics_reporter.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

namespace {

constexpr char kPhotoTransitionSmoothness[] =
    "Ash.AmbientMode.AnimationSmoothness.PhotoTransition";

void ReportSmoothness(int value) {
  base::UmaHistogramPercentage(kPhotoTransitionSmoothness, value);
}

gfx::ImageSkia ResizeImage(const gfx::ImageSkia& image,
                           const gfx::Size& view_size) {
  if (image.isNull())
    return gfx::ImageSkia();

  const double image_width = image.width();
  const double image_height = image.height();
  const double view_width = view_size.width();
  const double view_height = view_size.height();
  const double horizontal_ratio = view_width / image_width;
  const double vertical_ratio = view_height / image_height;
  const double image_ratio = image_height / image_width;
  const double view_ratio = view_height / view_width;

  // If the image and the container view has the same orientation, e.g. both
  // portrait, the |scale| will make the image filled the whole view with
  // possible cropping on one direction. If they are in different orientation,
  // the |scale| will display the image in the view without any cropping, but
  // with empty background.
  const double scale = (image_ratio - 1) * (view_ratio - 1) > 0
                           ? std::max(horizontal_ratio, vertical_ratio)
                           : std::min(horizontal_ratio, vertical_ratio);
  const gfx::Size& resized = gfx::ScaleToCeiledSize(image.size(), scale);
  return gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, resized);
}

}  // namespace

// PhotoView ------------------------------------------------------------------
PhotoView::PhotoView(AmbientViewDelegate* delegate) : delegate_(delegate) {
  DCHECK(delegate_);
  SetID(AssistantViewID::kAmbientPhotoView);
  Init();
}

PhotoView::~PhotoView() {
  delegate_->GetAmbientBackendModel()->RemoveObserver(this);
}

const char* PhotoView::GetClassName() const {
  return "PhotoView";
}

void PhotoView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  for (const int index : {0, 1}) {
    auto image = images_unscaled_[index];
    auto image_resized = ResizeImage(image, size());
    image_views_[index]->UpdateImage(image_resized);
  }
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
  SetLayoutManager(std::make_unique<views::FillLayout>());

  for (auto*& image_view : image_views_) {
    // Creates image views.
    image_view =
        AddChildView(std::make_unique<AmbientBackgroundImageView>(delegate_));
    // Each image view will be animated on its own layer.
    image_view->SetPaintToLayer();
    image_view->layer()->SetFillsBoundsOpaquely(false);
  }

  // Hides one image view initially for fade in animation.
  image_views_[1]->layer()->SetOpacity(0.0f);

  delegate_->GetAmbientBackendModel()->AddObserver(this);
}

void PhotoView::UpdateImages() {
  auto* model = delegate_->GetAmbientBackendModel();
  auto& next_image = model->GetNextImage();
  images_unscaled_[image_index_] = next_image.photo;
  if (images_unscaled_[image_index_].isNull())
    return;

  auto next_resized = ResizeImage(images_unscaled_[image_index_], size());
  image_views_[image_index_]->UpdateImage(next_resized);
  image_views_[image_index_]->UpdateImageDetails(
      base::UTF8ToUTF16(next_image.details));
  image_index_ = 1 - image_index_;
}

void PhotoView::StartTransitionAnimation() {
  ui::Layer* visible_layer = image_views_[image_index_]->layer();
  {
    ui::ScopedLayerAnimationSettings animation(visible_layer->GetAnimator());
    animation.SetTransitionDuration(kAnimationDuration);
    animation.SetTweenType(gfx::Tween::LINEAR);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    animation.CacheRenderSurface();

    ui::AnimationThroughputReporter reporter(
        animation.GetAnimator(),
        metrics_util::ForSmoothness(base::BindRepeating(ReportSmoothness)));

    visible_layer->SetOpacity(0.0f);
  }

  ui::Layer* invisible_layer = image_views_[1 - image_index_]->layer();
  {
    ui::ScopedLayerAnimationSettings animation(invisible_layer->GetAnimator());
    animation.SetTransitionDuration(kAnimationDuration);
    animation.SetTweenType(gfx::Tween::LINEAR);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    animation.CacheRenderSurface();
    // For simplicity, only observe one animation.
    animation.AddObserver(this);

    ui::AnimationThroughputReporter reporter(
        animation.GetAnimator(),
        metrics_util::ForSmoothness(base::BindRepeating(ReportSmoothness)));

    invisible_layer->SetOpacity(1.0f);
  }
}

void PhotoView::OnImplicitAnimationsCompleted() {
  UpdateImages();
  delegate_->OnPhotoTransitionAnimationCompleted();
}

bool PhotoView::NeedToAnimateTransition() const {
  // Can do transition animation if both two images in |images_unscaled_| are
  // not nullptr. Check the image index 1 is enough.
  return !images_unscaled_[1].isNull();
}

const gfx::ImageSkia& PhotoView::GetCurrentImagesForTesting() {
  return image_views_[image_index_]->GetCurrentImage();
}

}  // namespace ash
