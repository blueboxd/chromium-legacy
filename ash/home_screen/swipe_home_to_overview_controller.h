// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_SWIPE_HOME_TO_OVERVIEW_CONTROLLER_H_
#define ASH_HOME_SCREEN_SWIPE_HOME_TO_OVERVIEW_CONTROLLER_H_

#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/geometry/point.h"

namespace ash {

// Used by HomeLauncherGestureHandler to handle gesture drag events while the
// handler is in kSwipeHomeToOverview mode. The controller handles swipe gesture
// from hot seat on the home screen. The gesture, if detected, transitions the
// home screen to the overview UI.
class SwipeHomeToOverviewController {
 public:
  SwipeHomeToOverviewController();
  ~SwipeHomeToOverviewController();

  // Called during swiping up on the shelf.
  void Drag(const gfx::Point& location_in_screen,
            float scroll_x,
            float scroll_y);
  void EndDrag(const gfx::Point& location_in_screen,
               base::Optional<float> velocity_y);
  void CancelDrag();

 private:
  bool drag_started_ = false;
  gfx::Point initial_location_in_screen_;

  DISALLOW_COPY_AND_ASSIGN(SwipeHomeToOverviewController);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_SWIPE_HOME_TO_OVERVIEW_CONTROLLER_H_
