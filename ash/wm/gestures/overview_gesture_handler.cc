// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/overview_gesture_handler.h"

#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/metrics/user_metrics.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"

namespace ash {

// The threshold before engaging overview with a touchpad three-finger scroll.
const float OverviewGestureHandler::vertical_threshold_pixels_ = 300;

// The threshold before moving selector horizontally when using a touchpad
// three-finger scroll.
const float OverviewGestureHandler::horizontal_threshold_pixels_ = 330;

OverviewGestureHandler::OverviewGestureHandler() = default;

OverviewGestureHandler::~OverviewGestureHandler() = default;

bool OverviewGestureHandler::ProcessScrollEvent(const ui::ScrollEvent& event) {
  const int finger_count = event.finger_count();
  if (event.type() == ui::ET_SCROLL_FLING_START ||
      event.type() == ui::ET_SCROLL_FLING_CANCEL ||
      (finger_count != 3 && finger_count != 4)) {
    scroll_x_ = scroll_y_ = 0;
    return false;
  }

  scroll_x_ += event.x_offset();
  scroll_y_ += event.y_offset();

  // Horizontal 4-finger scroll switches desks if possible.
  if (finger_count == 4) {
    if (std::fabs(scroll_x_) >= std::fabs(scroll_y_)) {
      if (std::fabs(scroll_x_) < horizontal_threshold_pixels_)
        return false;

      const bool going_left = scroll_x_ > 0;
      scroll_x_ = scroll_y_ = 0;

      // This does not invert if the user changes their touchpad settings
      // currently. The scroll works Australian way (scroll left to go to the
      // desk on the right and vice versa).
      DesksController::Get()->ActivateAdjacentDesk(
          going_left, DesksSwitchSource::kDeskSwitchTouchpad);
      return true;
    }
  }

  OverviewController* overview_controller = Shell::Get()->overview_controller();

  // Horizontal 3-finger scroll moves selection when already in overview mode.
  if (std::fabs(scroll_x_) >= std::fabs(scroll_y_)) {
    if (!overview_controller->InOverviewSession()) {
      scroll_x_ = scroll_y_ = 0;
      return false;
    }
    if (std::fabs(scroll_x_) < horizontal_threshold_pixels_)
      return false;

    const int increment = scroll_x_ > 0 ? 1 : -1;
    scroll_x_ = scroll_y_ = 0;
    overview_controller->IncrementSelection(increment);
    return true;
  }

  // Use vertical 3-finger scroll gesture up to enter overview, down to exit.
  if (overview_controller->InOverviewSession()) {
    if (scroll_y_ < 0)
      scroll_x_ = scroll_y_ = 0;
    if (scroll_y_ < vertical_threshold_pixels_)
      return false;
  } else {
    if (scroll_y_ > 0)
      scroll_x_ = scroll_y_ = 0;
    if (scroll_y_ > -vertical_threshold_pixels_)
      return false;
  }

  // Reset scroll amount on toggling.
  scroll_x_ = scroll_y_ = 0;
  base::RecordAction(base::UserMetricsAction("Touchpad_Gesture_Overview"));
  if (overview_controller->InOverviewSession()) {
    if (overview_controller->AcceptSelection())
      return true;
    overview_controller->EndOverview();
  } else {
    overview_controller->StartOverview();
  }
  return true;
}

}  // namespace ash
