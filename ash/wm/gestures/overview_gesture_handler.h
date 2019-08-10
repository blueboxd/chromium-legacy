// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_OVERVIEW_GESTURE_HANDLER_H_
#define ASH_WM_GESTURES_OVERVIEW_GESTURE_HANDLER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/optional.h"

namespace ui {
class ScrollEvent;
}

namespace ash {

// This handles 3-finger touchpad scroll events to enter/exit overview mode and
// move the overview highlight if it is visible. This class also handles
// 4-finger horizontal scrolls to switch desks.
// TODO(sammiequon): Rename this class and do some cleanup.
class ASH_EXPORT OverviewGestureHandler {
 public:
  OverviewGestureHandler();
  virtual ~OverviewGestureHandler();

  // Processes a scroll event and may switch desks, start overview or move the
  // overivew highlight. Returns true if the event has been handled and should
  // not be processed further, false otherwise.
  bool ProcessScrollEvent(const ui::ScrollEvent& event);

 private:
  friend class OverviewGestureHandlerTest;

  // A struct containing the relevant data during a scroll session.
  struct ScrollData {
    int finger_count = 0;
    float scroll_x = 0.f;
    float scroll_y = 0.f;
  };

  // Called when a scroll is ended. Returns true if the scroll is processed.
  bool EndScroll();

  // Tries to move the overview selector. Returns true if successful. Called in
  // the middle of scrolls and when scrolls have ended.
  bool MoveOverviewSelection(int finger_count, float scroll_x, float scroll_y);

  // Contains the data during a scroll session. Empty is no scroll is underway.
  base::Optional<ScrollData> scroll_data_;

  // The threshold before engaging overview with a touchpad three-finger scroll.
  static const float vertical_threshold_pixels_;

  // The threshold before moving selector horizontally when using a touchpad
  // three-finger scroll.
  static const float horizontal_threshold_pixels_;

  DISALLOW_COPY_AND_ASSIGN(OverviewGestureHandler);
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_OVERVIEW_GESTURE_HANDLER_H_
