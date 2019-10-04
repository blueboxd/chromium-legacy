// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/back_gesture_affordance.h"

#include "components/vector_icons/vector_icons.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

namespace {

// Distance of the arrow that above the drag start position.
constexpr int kArrowAboveStartPosition = 64;

constexpr int kArrowSize = 20;

// Radius of the arrow's rounded background.
constexpr int kBackgroundRadius = 20;

// Radius of the ripple while affordance transform achieves |kMaxTransform|.
constexpr int kRippleMaxRadius = 32;

// Radius of the ripple while affordance transform achieves |kBurstTransform|.
constexpr int kRippleBurstRadius = 40;

// Transform for the affordance to go from outside of the display to the
// position that it needs to update the arrow and background colors.
constexpr float kMaxTransform = 100.f;

// |kMaxTransform| plus the transform for the ripple of the affordance to
// increase from |kRippleMaxRadius| to |kRippleBurstRadius|.
constexpr float kBurstTransform = 116.f;

constexpr base::TimeDelta kAbortAnimationTimeout =
    base::TimeDelta::FromMilliseconds(300);
constexpr base::TimeDelta kCompleteAnimationTimeout =
    base::TimeDelta::FromMilliseconds(200);

constexpr SkColor kRippleColor = SkColorSetA(gfx::kGoogleBlue600, 0x4C);  // 30%

}  // namespace

BackGestureAffordance::BackGestureAffordance(const gfx::Point& location)
    : painted_layer_(ui::LAYER_TEXTURED) {
  painted_layer_.SetFillsBoundsOpaquely(false);
  gfx::Rect painted_layer_bounds(
      gfx::Rect(2 * kRippleBurstRadius, 2 * kRippleBurstRadius));
  gfx::Point origin;
  // TODO(crbug.com/1002733): Handle the case while origin y is outside display.
  origin.set_y(location.y() - kArrowAboveStartPosition - kRippleBurstRadius);
  origin.set_x(-kRippleBurstRadius - kBackgroundRadius);
  painted_layer_bounds.set_origin(origin);
  painted_layer_.SetBounds(painted_layer_bounds);

  painted_layer_.set_delegate(this);
}

BackGestureAffordance::~BackGestureAffordance() {}

void BackGestureAffordance::SetDragProgress(int x_location) {
  DCHECK_EQ(State::DRAGGING, state_);
  DCHECK_LE(0.f, drag_progress_);

  const float progress = (x_location + kBackgroundRadius) / kMaxTransform;
  if (drag_progress_ == progress)
    return;
  drag_progress_ = std::min(progress, kBurstTransform / kMaxTransform);

  UpdateTransform();
  SchedulePaint();
}

void BackGestureAffordance::Abort() {
  DCHECK_EQ(State::DRAGGING, state_);

  state_ = State::ABORTING;
  animation_ = std::make_unique<gfx::LinearAnimation>(
      drag_progress_ * kAbortAnimationTimeout,
      gfx::LinearAnimation::kDefaultFrameRate, this);
  animation_->Start();
}

void BackGestureAffordance::Complete() {
  DCHECK_EQ(State::DRAGGING, state_);
  DCHECK_LE(1.f, drag_progress_);

  state_ = State::COMPLETING;

  animation_ = std::make_unique<gfx::LinearAnimation>(
      kCompleteAnimationTimeout, gfx::LinearAnimation::kDefaultFrameRate, this);
  animation_->Start();
}

void BackGestureAffordance::UpdateTransform() {
  const float offset = (1 - abort_progress_) * drag_progress_ * kMaxTransform;
  gfx::Transform transform;
  transform.Translate(offset, 0);
  painted_layer_.SetTransform(transform);
}

void BackGestureAffordance::SchedulePaint() {
  painted_layer_.SchedulePaint(gfx::Rect(painted_layer_.size()));
}

void BackGestureAffordance::SetAbortProgress(float progress) {
  DCHECK_EQ(State::ABORTING, state_);
  DCHECK_LE(0.f, progress);
  DCHECK_GE(1.f, progress);

  if (abort_progress_ == progress)
    return;
  abort_progress_ = progress;

  UpdateTransform();
  SchedulePaint();
}

void BackGestureAffordance::SetCompleteProgress(float progress) {
  DCHECK_EQ(State::COMPLETING, state_);
  DCHECK_LE(0.f, progress);
  DCHECK_GE(1.f, progress);

  if (complete_progress_ == progress)
    return;
  complete_progress_ = progress;

  painted_layer_.SetOpacity(gfx::Tween::CalculateValue(
      gfx::Tween::FAST_OUT_SLOW_IN, 1 - complete_progress_));

  SchedulePaint();
}

void BackGestureAffordance::OnPaintLayer(const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, painted_layer_.size());
  gfx::Canvas* canvas = recorder.canvas();

  const gfx::PointF center_point(kRippleBurstRadius, kRippleBurstRadius);
  const float progress = (1 - abort_progress_) * drag_progress_;

  // Draw the ripple.
  cc::PaintFlags ripple_flags;
  ripple_flags.setAntiAlias(true);
  ripple_flags.setStyle(cc::PaintFlags::kFill_Style);
  ripple_flags.setColor(kRippleColor);

  const bool exceed_full_progress = progress >= 1.f;
  float ripple_radius = 0.f;
  if (exceed_full_progress) {
    const float factor = (kRippleBurstRadius - kRippleMaxRadius) /
                         (kBurstTransform / kMaxTransform - 1);
    ripple_radius = (kRippleMaxRadius - factor) + factor * progress;
  } else {
    ripple_radius =
        kBackgroundRadius + progress * (kRippleMaxRadius - kBackgroundRadius);
  }
  canvas->DrawCircle(center_point, ripple_radius, ripple_flags);

  // Draw the arrow background circle.
  cc::PaintFlags bg_flags;
  bg_flags.setAntiAlias(true);
  bg_flags.setStyle(cc::PaintFlags::kFill_Style);

  bg_flags.setColor(exceed_full_progress ? gfx::kGoogleBlue600 : SK_ColorWHITE);
  canvas->DrawCircle(center_point, kBackgroundRadius, bg_flags);

  // Draw the arrow.
  const float arrow_x = center_point.x() - kArrowSize / 2.f;
  const float arrow_y = center_point.y() - kArrowSize / 2.f;
  if (exceed_full_progress) {
    canvas->DrawImageInt(gfx::CreateVectorIcon(vector_icons::kBackArrowIcon,
                                               kArrowSize, SK_ColorWHITE),
                         static_cast<int>(arrow_x), static_cast<int>(arrow_y));
  } else {
    canvas->DrawImageInt(gfx::CreateVectorIcon(vector_icons::kBackArrowIcon,
                                               kArrowSize, gfx::kGoogleBlue600),
                         static_cast<int>(arrow_x), static_cast<int>(arrow_y));
  }
}

void BackGestureAffordance::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {}

void BackGestureAffordance::AnimationEnded(const gfx::Animation* animation) {}

void BackGestureAffordance::AnimationProgressed(
    const gfx::Animation* animation) {
  switch (state_) {
    case State::DRAGGING:
      NOTREACHED();
      break;
    case State::ABORTING:
      SetAbortProgress(animation->GetCurrentValue());
      break;
    case State::COMPLETING:
      SetCompleteProgress(animation->GetCurrentValue());
      break;
  }
}

void BackGestureAffordance::AnimationCanceled(const gfx::Animation* animation) {
  NOTREACHED();
}

}  // namespace ash
