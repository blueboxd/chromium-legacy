// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_transition_animator.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/metrics/histogram_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class HotseatTransitionAnimator::TransitionAnimationMetricsReporter
    : public ui::AnimationMetricsReporter {
 public:
  TransitionAnimationMetricsReporter() = default;
  ~TransitionAnimationMetricsReporter() override = default;

  void set_new_state(HotseatState new_state) { new_state_ = new_state; }

  // ui::AnimationMetricsReporter:
  void Report(int value) override {
    switch (new_state_) {
      case HotseatState::kShown:
        UMA_HISTOGRAM_PERCENTAGE(
            "Ash.HotseatTransition.AnimationSmoothness."
            "TransitionToShownHotseat",
            value);
        break;
      case HotseatState::kExtended:
        UMA_HISTOGRAM_PERCENTAGE(
            "Ash.HotseatTransition.AnimationSmoothness."
            "TransitionToExtendedHotseat",
            value);
        break;
      case HotseatState::kHidden:
        UMA_HISTOGRAM_PERCENTAGE(
            "Ash.HotseatTransition.AnimationSmoothness."
            "TransitionToHiddenHotseat",
            value);
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  // The state to which the animation is transitioning.
  HotseatState new_state_;
};

HotseatTransitionAnimator::HotseatTransitionAnimator(ShelfWidget* shelf_widget)
    : shelf_widget_(shelf_widget),
      animation_metrics_reporter_(
          std::make_unique<TransitionAnimationMetricsReporter>()) {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

HotseatTransitionAnimator::~HotseatTransitionAnimator() {
  StopObservingImplicitAnimations();
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
}

void HotseatTransitionAnimator::OnHotseatStateChanged(HotseatState old_state,
                                                      HotseatState new_state) {
  DoAnimation(old_state, new_state);
}

void HotseatTransitionAnimator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HotseatTransitionAnimator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HotseatTransitionAnimator::OnImplicitAnimationsCompleted() {
  std::move(animation_complete_callback_).Run();

  if (test_observer_)
    test_observer_->OnTransitionTestAnimationEnded();
}

void HotseatTransitionAnimator::OnTabletModeStarting() {
  tablet_mode_transitioning_ = true;
}

void HotseatTransitionAnimator::OnTabletModeStarted() {
  tablet_mode_transitioning_ = false;
}

void HotseatTransitionAnimator::OnTabletModeEnding() {
  tablet_mode_transitioning_ = true;
}

void HotseatTransitionAnimator::OnTabletModeEnded() {
  tablet_mode_transitioning_ = false;
}

void HotseatTransitionAnimator::SetAnimationsEnabledInSessionState(
    bool enabled) {
  animations_enabled_for_current_session_state_ = enabled;

  ui::Layer* animating_background = shelf_widget_->GetAnimatingBackground();
  if (!enabled && animating_background->GetAnimator()->is_animating())
    animating_background->GetAnimator()->StopAnimating();
}

void HotseatTransitionAnimator::SetTestObserver(TestObserver* test_observer) {
  test_observer_ = test_observer;
}

void HotseatTransitionAnimator::DoAnimation(HotseatState old_state,
                                            HotseatState new_state) {
  if (!ShouldDoAnimation(old_state, new_state))
    return;

  StopObservingImplicitAnimations();

  const bool animating_to_shown_hotseat = new_state == HotseatState::kShown;

  shelf_widget_->GetAnimatingBackground()->SetColor(
      ShelfConfig::Get()->GetMaximizedShelfColor());

  gfx::Rect target_bounds = shelf_widget_->GetOpaqueBackground()->bounds();
  target_bounds.set_height(ShelfConfig::Get()->in_app_shelf_size());
  target_bounds.set_y(
      animating_to_shown_hotseat ? ShelfConfig::Get()->system_shelf_size() : 0);
  shelf_widget_->GetAnimatingBackground()->SetBounds(target_bounds);
  shelf_widget_->GetAnimatingDragHandle()->SetBounds(
      shelf_widget_->GetDragHandle()->bounds());

  int starting_y;
  // This animation is triggered after bounds have been set in the shelf, or
  // while the shelf is beginning to animate to new bounds. To prevent the
  // background from jumping in either case, adjust the y position to account
  // for the current size of the |shelf_widget_|.
  if (animating_to_shown_hotseat) {
    starting_y = shelf_widget_->GetWindowBoundsInScreen().height() -
                 ShelfConfig::Get()->in_app_shelf_size();
  } else {
    starting_y = shelf_widget_->GetWindowBoundsInScreen().height();
  }
  gfx::Transform transform;
  const int y_offset = starting_y - target_bounds.y();
  transform.Translate(0, y_offset);
  shelf_widget_->GetAnimatingBackground()->SetTransform(transform);
  animation_metrics_reporter_->set_new_state(new_state);

  for (auto& observer : observers_)
    observer.OnHotseatTransitionAnimationStarted(old_state, new_state);

  {
    ui::ScopedLayerAnimationSettings shelf_bg_animation_setter(
        shelf_widget_->GetAnimatingBackground()->GetAnimator());
    shelf_bg_animation_setter.SetTransitionDuration(
        ShelfConfig::Get()->hotseat_background_animation_duration());
    shelf_bg_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
    shelf_bg_animation_setter.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    shelf_bg_animation_setter.SetAnimationMetricsReporter(
        animation_metrics_reporter_.get());
    animation_complete_callback_ = base::BindOnce(
        &HotseatTransitionAnimator::NotifyHotseatTransitionAnimationEnded,
        weak_ptr_factory_.GetWeakPtr(), old_state, new_state);
    shelf_bg_animation_setter.AddObserver(this);

    shelf_widget_->GetAnimatingBackground()->SetTransform(gfx::Transform());
  }
}

bool HotseatTransitionAnimator::ShouldDoAnimation(HotseatState old_state,
                                                  HotseatState new_state) {
  // The first HotseatState change when going to tablet mode should not be
  // animated.
  if (tablet_mode_transitioning_)
    return false;

  if (!animations_enabled_for_current_session_state_)
    return false;

  return (new_state == HotseatState::kShown ||
          old_state == HotseatState::kShown) &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

void HotseatTransitionAnimator::NotifyHotseatTransitionAnimationEnded(
    HotseatState old_state,
    HotseatState new_state) {
  for (auto& observer : observers_)
    observer.OnHotseatTransitionAnimationEnded(old_state, new_state);
}

}  // namespace ash
