// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/swipe_home_to_overview_controller.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/optional.h"

namespace ash {

namespace {

// The minimum vertical difference between the gesture start and end position
// for the gesture to register as a swipe from home to overview screen.
constexpr int kSwipeMovementThreshold = 100;

// The minimum ending vertical gesture velocity for the gesture to register as
// a swipe from home to overview screen.
constexpr int kSwipeVelocityThreshold = 1000;

}  // namespace

SwipeHomeToOverviewController::SwipeHomeToOverviewController() {}

SwipeHomeToOverviewController::~SwipeHomeToOverviewController() {
  CancelDrag();
}

void SwipeHomeToOverviewController::Drag(const gfx::Point& location_in_screen,
                                         float scroll_x,
                                         float scroll_y) {
  if (drag_started_)
    return;

  initial_location_in_screen_ = location_in_screen;
  drag_started_ = true;
}

void SwipeHomeToOverviewController::EndDrag(
    const gfx::Point& location_in_screen,
    base::Optional<float> velocity_y) {
  if (!drag_started_)
    return;

  drag_started_ = false;
  if (location_in_screen.y() - initial_location_in_screen_.y() <=
          -kSwipeMovementThreshold &&
      velocity_y.value_or(0) < -kSwipeVelocityThreshold) {
    Shell::Get()->overview_controller()->StartOverview();
  }
}

void SwipeHomeToOverviewController::CancelDrag() {
  if (!drag_started_)
    return;

  initial_location_in_screen_ = gfx::Point();
  drag_started_ = false;
}

}  // namespace ash
