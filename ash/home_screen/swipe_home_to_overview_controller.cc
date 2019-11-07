// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/swipe_home_to_overview_controller.h"

#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/numerics/ranges.h"
#include "base/optional.h"
#include "base/time/default_tick_clock.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/tween.h"

namespace ash {

namespace {

// The target/min home launcher view scale.
constexpr float kTargetHomeScale = 0.92f;

// The home UI will be scaled down towards center of the screen as drag location
// approaches the threshold for the transition to overview. The target threshold
// for scaling is extended above the actual threshold to make the UI change more
// noticable to the user as drag approaches the threshold - this is the ratio
// by which the projecrted threshold is extended.
constexpr float kExtendedVerticalThresholdForOverviewTransitionRatio = 0.5f;

// The amount of time the drag has to remain still before the transition to the
// overview starts.
constexpr base::TimeDelta kOverviewTransitionDelay =
    base::TimeDelta::FromMilliseconds(350);

// The duration of transition from the home screen current scaled state to the
// initial (unscaled) state when the gesture is canceled.
constexpr base::TimeDelta kGestureCancelationDuration =
    base::TimeDelta::FromMilliseconds(350);

void UpdateHomeAnimationForGestureCancel(
    ui::ScopedLayerAnimationSettings* settings) {
  settings->SetTransitionDuration(kGestureCancelationDuration);
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

}  // namespace

SwipeHomeToOverviewController::SwipeHomeToOverviewController(int64_t display_id)
    : SwipeHomeToOverviewController(display_id,
                                    base::DefaultTickClock::GetInstance()) {}

SwipeHomeToOverviewController::SwipeHomeToOverviewController(
    int64_t display_id,
    const base::TickClock* tick_clock)
    : display_id_(display_id), overview_transition_timer_(tick_clock) {}

SwipeHomeToOverviewController::~SwipeHomeToOverviewController() {
  CancelDrag();
}

void SwipeHomeToOverviewController::Drag(const gfx::Point& location_in_screen,
                                         float scroll_x,
                                         float scroll_y) {
  if (state_ == State::kFinished)
    return;

  display::Display display;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_,
                                                             &display)) {
    CancelDrag();
    return;
  }

  const int shelf_top =
      display.bounds().bottom() - ShelfConfig::Get()->shelf_size();
  if (state_ == State::kInitial) {
    // Do not start drag until the drag goes above the shelf.
    if (location_in_screen.y() > shelf_top)
      return;

    overview_transition_threshold_y_ =
        shelf_top - kVerticalThresholdForOverviewTransition;
    state_ = State::kTrackingDrag;
  } else {
    const bool over_overview_transition_threshold =
        location_in_screen.y() <= overview_transition_threshold_y_;

    // Ignore small drag updates, unless they move the drag above the overview
    // transition threshold.
    if (std::abs(location_in_screen.y() - last_location_in_screen_.y()) <
            kMovementThreshold &&
        (!over_overview_transition_threshold ||
         overview_transition_timer_.IsRunning())) {
      return;
    }

    if (over_overview_transition_threshold)
      ScheduleFinalizeDragAndShowOverview();
    else
      overview_transition_timer_.Stop();
  }

  last_location_in_screen_ = location_in_screen;

  // Update the home screen scale to match progress during the drag.
  // Use extended threshold as the projected final transition position - UI
  // changing even after the user gets over the threshold should make the user
  // more likely to keep dragging up when they get really close to the threshold
  // for transition to overview (and reduce false negatives for detecting
  // transition to overview).
  const float extended_threshold =
      overview_transition_threshold_y_ -
      kVerticalThresholdForOverviewTransition *
          kExtendedVerticalThresholdForOverviewTransitionRatio;

  const float distance = location_in_screen.y() - extended_threshold;
  const float target_distance =
      kVerticalThresholdForOverviewTransition *
      (1 + kExtendedVerticalThresholdForOverviewTransitionRatio);

  const float progress = gfx::Tween::CalculateValue(
      gfx::Tween::FAST_OUT_SLOW_IN,
      base::ClampToRange(1 - distance / target_distance, 0.0f, 1.0f));

  float scale = gfx::Tween::FloatValueBetween(progress, 1.0f, kTargetHomeScale);
  Shell::Get()
      ->home_screen_controller()
      ->delegate()
      ->UpdateScaleAndOpacityForHomeLauncher(scale, 1.0f /*opacity*/,
                                             base::nullopt /*animation_info*/,
                                             base::NullCallback());
}

void SwipeHomeToOverviewController::EndDrag(
    const gfx::Point& location_in_screen,
    base::Optional<float> velocity_y) {
  // Overview is triggered by |overview_transition_timer_|. If EndDrag()
  // is called before the timer fires, the gesture is canceled.
  CancelDrag();
}

void SwipeHomeToOverviewController::CancelDrag() {
  if (state_ != State::kTrackingDrag) {
    state_ = State::kFinished;
    return;
  }

  overview_transition_timer_.Stop();
  last_location_in_screen_ = gfx::Point();
  overview_transition_threshold_y_ = 0;
  state_ = State::kFinished;

  Shell::Get()
      ->home_screen_controller()
      ->delegate()
      ->UpdateScaleAndOpacityForHomeLauncher(
          1.0f /*scale*/, 1.0f /*opacity*/, base::nullopt /*animation_info*/,
          base::BindRepeating(&UpdateHomeAnimationForGestureCancel));
}

void SwipeHomeToOverviewController::ScheduleFinalizeDragAndShowOverview() {
  overview_transition_timer_.Start(
      FROM_HERE, kOverviewTransitionDelay,
      base::BindOnce(
          &SwipeHomeToOverviewController::FinalizeDragAndShowOverview,
          base::Unretained(this)));
}

void SwipeHomeToOverviewController::FinalizeDragAndShowOverview() {
  last_location_in_screen_ = gfx::Point();
  state_ = State::kFinished;
  overview_transition_threshold_y_ = 0;

  Shell::Get()->overview_controller()->StartOverview();
}

}  // namespace ash
