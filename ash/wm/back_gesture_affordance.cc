// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/back_gesture_affordance.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/aura/window.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Distance from the arrow to the drag touch point. The arrow is usually
// above the touch point but will be put below the touch point if the affordance
// is outside of the display.
constexpr int kDistanceFromArrowToTouchPoint = 64;

constexpr int kArrowSize = 20;

// Radius of the arrow's rounded background.
constexpr int kBackgroundRadius = 20;

// Radius of the ripple while x-axis movement of the affordance achieves
// |kDistanceForFullProgress|.
constexpr int kRippleMaxRadius = 32;

// Radius of the ripple while x-axis movement of the affordance achieves
// |kDistanceForBurstRipple|.
constexpr int kRippleBurstRadius = 40;

// Drag distance for full drag progress. Affordance also needs to update its
// arrow and background colors while its x-axis movement achieves this.
constexpr float kDistanceForFullProgress = 100.f;

// |kDistanceForFullProgress| plus the x-axis distance for the ripple of the
// affordance to increase from |kRippleMaxRadius| to |kRippleBurstRadius|.
constexpr float kDistanceForBurstRipple = 116.f;

// Drag distance to achieve the max drag progress.
constexpr float kDistanceForMaxDragProgress = 260.f;

constexpr float kMaxDragProgress =
    kDistanceForMaxDragProgress / kDistanceForFullProgress;

constexpr base::TimeDelta kAbortAnimationTimeout =
    base::TimeDelta::FromMilliseconds(300);
constexpr base::TimeDelta kCompleteAnimationTimeout =
    base::TimeDelta::FromMilliseconds(200);

constexpr SkColor kRippleColor = SkColorSetA(gfx::kGoogleBlue600, 0x4C);  // 30%

// Y-axis drag distance to achieve full y drag progress.
constexpr float kDistanceForFullYProgress = 80.f;

// Maximium y-axis movement of the affordance. Note, the affordance can move
// both up and down.
constexpr float kMaxYMovement = 8.f;

class AffordanceView : public views::View {
 public:
  AffordanceView() {
    SetPaintToLayer(ui::LAYER_TEXTURED);
    layer()->SetFillsBoundsOpaquely(false);
  }

  ~AffordanceView() override = default;

  // Schedule painting on given |drag_progress| and |abort_progress|.
  void Paint(float drag_progress, float abort_progress) {
    drag_progress_ = drag_progress;
    abort_progress_ = abort_progress;
    SchedulePaint();
  }

 private:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override {
    views::View::OnPaint(canvas);

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
      const float factor =
          (kRippleBurstRadius - kRippleMaxRadius) / (kMaxDragProgress - 1);
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

    bg_flags.setColor(exceed_full_progress ? gfx::kGoogleBlue600
                                           : SK_ColorWHITE);
    canvas->DrawCircle(center_point, kBackgroundRadius, bg_flags);

    // Draw the arrow.
    const float arrow_x = center_point.x() - kArrowSize / 2.f;
    const float arrow_y = center_point.y() - kArrowSize / 2.f;
    if (exceed_full_progress) {
      canvas->DrawImageInt(gfx::CreateVectorIcon(vector_icons::kBackArrowIcon,
                                                 kArrowSize, SK_ColorWHITE),
                           static_cast<int>(arrow_x),
                           static_cast<int>(arrow_y));
    } else {
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(vector_icons::kBackArrowIcon, kArrowSize,
                                gfx::kGoogleBlue600),
          static_cast<int>(arrow_x), static_cast<int>(arrow_y));
    }
  }

  float drag_progress_ = 0.f;
  float abort_progress_ = 0.f;

  DISALLOW_COPY_AND_ASSIGN(AffordanceView);
};

// Get the bounds of the affordance widget, which is outside of the left edge of
// the display.
gfx::Rect GetWidgetBounds(const gfx::Point& location) {
  gfx::Rect widget_bounds(
      gfx::Rect(2 * kRippleBurstRadius, 2 * kRippleBurstRadius));
  gfx::Point origin;
  origin.set_x(-kRippleBurstRadius - kBackgroundRadius);
  int origin_y =
      location.y() - kDistanceFromArrowToTouchPoint - kRippleBurstRadius;
  if (origin_y < 0) {
    origin_y =
        location.y() + kDistanceFromArrowToTouchPoint - kRippleBurstRadius;
  }
  origin.set_y(origin_y);
  widget_bounds.set_origin(origin);
  return widget_bounds;
}

}  // namespace

BackGestureAffordance::BackGestureAffordance(const gfx::Point& location) {
  CreateAffordanceWidget(location);
}

BackGestureAffordance::~BackGestureAffordance() {}

void BackGestureAffordance::SetDragProgress(int x_drag_amount,
                                            int y_drag_amount) {
  DCHECK_EQ(State::DRAGGING, state_);
  DCHECK_LE(0.f, drag_progress_);

  // Since affordance is put outside of the display, add the distance from its
  // center point to the left edge of the display to be the actual drag
  // distance.
  float progress =
      (x_drag_amount + kBackgroundRadius) / kDistanceForFullProgress;
  progress = std::min(progress, kMaxDragProgress);

  float y_progress = y_drag_amount / kDistanceForFullYProgress;
  y_progress = std::min(1.0f, std::max(-1.0f, y_progress));

  if (drag_progress_ == progress && y_progress == y_drag_progress_)
    return;
  drag_progress_ = progress;
  y_drag_progress_ = y_progress;

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

void BackGestureAffordance::CreateAffordanceWidget(const gfx::Point& location) {
  affordance_widget_ = std::make_unique<views::Widget>();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = true;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "BackGestureAffordance";
  params.activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  params.parent = window_util::GetRootWindowAt(location)->GetChildById(
      kShellWindowId_AlwaysOnTopContainer);
  affordance_widget_->Init(std::move(params));
  affordance_widget_->SetContentsView(new AffordanceView());
  affordance_widget_->SetBounds(GetWidgetBounds(location));
  affordance_widget_->Show();
  affordance_widget_->SetOpacity(1.f);
}

void BackGestureAffordance::UpdateTransform() {
  const float progress = (1 - abort_progress_) * drag_progress_;
  float offset = 0.f;
  if (progress >= 1.f) {
    const float factor = (kDistanceForBurstRipple - kDistanceForFullProgress) /
                         (kMaxDragProgress - 1.f);
    offset = (kDistanceForFullProgress - factor) + factor * progress;
  } else {
    offset = progress * kDistanceForFullProgress;
  }

  float y_offset = kMaxYMovement * y_drag_progress_;
  gfx::Transform transform;
  transform.Translate(offset, y_offset);
  affordance_widget_->GetContentsView()->SetTransform(transform);
}

void BackGestureAffordance::SchedulePaint() {
  static_cast<AffordanceView*>(affordance_widget_->GetContentsView())
      ->Paint(drag_progress_, abort_progress_);
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

  affordance_widget_->GetContentsView()->layer()->SetOpacity(
      gfx::Tween::CalculateValue(gfx::Tween::FAST_OUT_SLOW_IN,
                                 1 - complete_progress_));

  SchedulePaint();
}

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
