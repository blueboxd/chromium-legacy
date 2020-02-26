// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/drag_handle.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/timer/timer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

namespace {

// Vertical padding to make the drag handle easier to tap.
constexpr int kVerticalClickboxPadding = 15;

// Drag handle translation distance for the first part of nudge animation.
constexpr int kDragHandleNudgeVerticalMarginRise = -4;

// Drag handle translation distance for the second part of nudge animation.
// Also the translation distance for the ContextualNudge text box.
constexpr int kDragHandleNudgeVerticalMarginDrop = 8;

// Animation time for each translation of drag handle to show contextual nudge.
constexpr base::TimeDelta kDragHandleAnimationTime =
    base::TimeDelta::FromMilliseconds(300);

// Animation time to return drag handle to original position after hiding
// contextual nudge.
constexpr base::TimeDelta kDragHandleAnimationHideTime =
    base::TimeDelta::FromMilliseconds(600);

// Delay between animating drag handle and tooltip opacity.
constexpr base::TimeDelta kDragHandleNudgeOpacityDelay =
    base::TimeDelta::FromMilliseconds(500);

// Fade in time for drag handle nudge tooltip.
constexpr base::TimeDelta kDragHandleNudgeOpacityAnimationDuration =
    base::TimeDelta::FromMilliseconds(200);

// This class is deleted after OnImplicitAnimationsCompleted() is called.
class HideNudgeObserver : public ui::ImplicitAnimationObserver {
 public:
  HideNudgeObserver(ContextualNudge* drag_handle_nudge)
      : drag_handle_nudge_(drag_handle_nudge) {}
  ~HideNudgeObserver() override = default;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    drag_handle_nudge_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kUnspecified);
    delete this;
  }

 private:
  ContextualNudge* drag_handle_nudge_;
};

}  // namespace

DragHandle::DragHandle(AshColorProvider::RippleAttributes ripple_attributes,
                       int drag_handle_corner_radius) {
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetColor(ripple_attributes.base_color);
  // TODO(manucornet): Figure out why we need a manual opacity adjustment
  // to make this color look the same as the status area highlight.
  layer()->SetOpacity(ripple_attributes.inkdrop_opacity + 0.075);
  layer()->SetRoundedCornerRadius(
      {drag_handle_corner_radius, drag_handle_corner_radius,
       drag_handle_corner_radius, drag_handle_corner_radius});
  SetSize(ShelfConfig::Get()->DragHandleSize());
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

DragHandle::~DragHandle() = default;

bool DragHandle::DoesIntersectRect(const views::View* target,
                                   const gfx::Rect& rect) const {
  DCHECK_EQ(target, this);
  gfx::Rect drag_handle_bounds = target->GetLocalBounds();
  drag_handle_bounds.set_y(drag_handle_bounds.y() - kVerticalClickboxPadding);
  drag_handle_bounds.set_height(drag_handle_bounds.height() +
                                2 * kVerticalClickboxPadding);
  return drag_handle_bounds.Intersects(rect);
}

void DragHandle::ShowDragHandleNudge(base::TimeDelta nudge_duration) {
  if (showing_nudge_)
    return;
  showing_nudge_ = true;

  AnimateDragHandleShow();
  ShowDragHandleTooltip();

  if (!nudge_duration.is_zero()) {
    hide_drag_handle_nudge_timer_.Start(
        FROM_HERE, nudge_duration,
        base::BindOnce(&DragHandle::HideDragHandleNudge,
                       base::Unretained(this)));
  }
}

void DragHandle::HideDragHandleNudge() {
  if (!showing_nudge_)
    return;
  hide_drag_handle_nudge_timer_.Stop();
  HideDragHandleNudgeHelper();
  showing_nudge_ = false;
}

void DragHandle::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::ET_GESTURE_TAP &&
      features::AreContextualNudgesEnabled()) {
    // Drag handle always shows nudge when tapped and does not affect the next
    // time a session based nudge will be shown.
    ShowDragHandleNudge(contextual_tooltip::kNudgeShowDuration);
  }
}

void DragHandle::ShowDragHandleTooltip() {
  DCHECK(!drag_handle_nudge_);
  drag_handle_nudge_ = new ContextualNudge(
      this, l10n_util::GetStringUTF16(IDS_ASH_DRAG_HANDLE_NUDGE));
  drag_handle_nudge_->GetWidget()->Show();
  drag_handle_nudge_->label()->layer()->SetOpacity(0.0f);

  {
    // Layer transform should be animated after a delay so the animator must
    // first schedules a pause for transform animation.
    ui::LayerAnimator* transform_animator =
        drag_handle_nudge_->GetWidget()->GetLayer()->GetAnimator();
    transform_animator->SchedulePauseForProperties(
        kDragHandleAnimationTime, ui::LayerAnimationElement::TRANSFORM);

    // Enqueue transform animation to start after pause.
    ui::ScopedLayerAnimationSettings transform_animation_settings(
        transform_animator);
    transform_animation_settings.SetTweenType(gfx::Tween::EASE_IN_OUT);
    transform_animation_settings.SetTransitionDuration(
        kDragHandleAnimationTime);
    transform_animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);

    // gfx::Transform translate;
    gfx::Transform translate;
    translate.Translate(0, kDragHandleNudgeVerticalMarginDrop);
    drag_handle_nudge_->GetWidget()->GetLayer()->SetTransform(translate);
  }

  {
    // Layer opacity should be animated after a delay so the animator must first
    // schedules a pause for opacity animation.
    ui::LayerAnimator* opacity_animator =
        drag_handle_nudge_->label()->layer()->GetAnimator();
    opacity_animator->SchedulePauseForProperties(
        kDragHandleNudgeOpacityDelay, ui::LayerAnimationElement::OPACITY);

    // Enqueue opacity animation to start after pause.
    ui::ScopedLayerAnimationSettings opacity_animation_settings(
        opacity_animator);
    opacity_animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    opacity_animation_settings.SetTweenType(gfx::Tween::LINEAR);
    opacity_animation_settings.SetTransitionDuration(
        kDragHandleNudgeOpacityAnimationDuration);
    drag_handle_nudge_->label()->layer()->SetOpacity(1.0f);
  }
}

void DragHandle::HideDragHandleNudgeHelper() {
  ScheduleDragHandleTranslationAnimation(
      0, kDragHandleAnimationHideTime,
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  if (drag_handle_nudge_) {
    ui::LayerAnimator* opacity_animator =
        drag_handle_nudge_->label()->layer()->GetAnimator();
    ui::ScopedLayerAnimationSettings opacity_animation_settings(
        opacity_animator);
    opacity_animation_settings.SetPreemptionStrategy(
        ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
    opacity_animation_settings.SetTweenType(gfx::Tween::LINEAR);
    opacity_animation_settings.SetTransitionDuration(
        kDragHandleNudgeOpacityAnimationDuration);

    // Register an animation observer to close the tooltip widget once the label
    // opacity is animated to 0 as the widget will no longer be needed after
    // this point.
    opacity_animation_settings.AddObserver(
        new HideNudgeObserver(drag_handle_nudge_));
    drag_handle_nudge_->label()->layer()->SetOpacity(0.0f);

    drag_handle_nudge_ = nullptr;
  }
}

void DragHandle::AnimateDragHandleShow() {
  // Drag handle is animated in two steps that run in sequence. The first step
  // uses |IMMEDIATELY_ANIMATE_TO_NEW_TARGET| to preempt any in-progress
  // animations while the second step uses |ENQUEUE_NEW_ANIMATION| so it runs
  // after the first animation.
  ScheduleDragHandleTranslationAnimation(
      kDragHandleNudgeVerticalMarginRise, kDragHandleAnimationTime,
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  ScheduleDragHandleTranslationAnimation(
      kDragHandleNudgeVerticalMarginDrop, kDragHandleAnimationTime,
      ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
}

void DragHandle::ScheduleDragHandleTranslationAnimation(
    int vertical_offset,
    base::TimeDelta animation_time,
    ui::LayerAnimator::PreemptionStrategy strategy) {
  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTweenType(gfx::Tween::EASE_IN_OUT);
  animation.SetTransitionDuration(animation_time);
  animation.SetPreemptionStrategy(strategy);

  gfx::Transform translate;
  translate.Translate(0, vertical_offset);
  SetTransform(translate);
}

}  // namespace ash
