// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_HOME_SCREEN_DRAG_WINDOW_FROM_SHELF_CONTROLLER_H_
#define ASH_HOME_SCREEN_DRAG_WINDOW_FROM_SHELF_CONTROLLER_H_

#include <vector>

#include "ash/public/cpp/window_properties.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "base/macros.h"
#include "base/optional.h"
#include "ui/gfx/geometry/point.h"

namespace aura {
class Window;
}

namespace ash {

// Only used by HomeLauncherGestureHandler::Mode::kDragWindowToHomeOrOverview
// mode. It's the window drag controller for a window that's being dragging from
// swiping up from the shelf.
class DragWindowFromShelfController {
 public:
  // The distance for the dragged window to pass over shelf so that it can be
  // dragged into home launcher or overview. If not pass this value, the window
  // will snap back to its original position.
  static constexpr float kReturnToMaximizedThreshold = 116;

  // The deceleration threshold to show overview during window dragging when
  // dragging a window up from the shelf.
  static constexpr float kShowOverviewThreshold = 4.f;

  // The upward velocity threshold to take the user to the home launcher screen
  // when swiping up from the shelf. Can happen anytime during dragging.
  static constexpr float kVelocityToHomeScreenThreshold = 1000.f;

  // The upward velocity threshold to fling the window into overview when split
  // view is active during dragging.
  static constexpr float kVelocityToOverviewThreshold = 1000.f;

  DragWindowFromShelfController(
      aura::Window* window,
      const std::vector<aura::Window*>& hidden_windows);
  ~DragWindowFromShelfController();

  // Called during swiping up on the shelf.
  void Drag(const gfx::Point& location_in_screen, float scroll_y);
  void EndDrag(const gfx::Point& location_in_screen,
               base::Optional<float> velocity_y);
  void CancelDrag();

 private:
  void OnDragStarted(const gfx::Point& location_in_screen);
  void OnDragEnded(const gfx::Point& location_in_screen,
                   bool should_drop_window_in_overview,
                   SplitViewController::SnapPosition snap_position);

  // Updates the dragged window's transform during dragging.
  void UpdateDraggedWindow(const gfx::Point& location_in_screen);

  // Returns the desired snap position on |location_in_screen| during dragging.
  SplitViewController::SnapPosition GetSnapPosition(
      const gfx::Point& location_in_screen) const;

  // Returns the desired indicator state on |location_in_screen|.
  IndicatorState GetIndicatorState(const gfx::Point& location_in_screen) const;

  // Returns true if the dragged window should restore to its original bounds
  // after drag ends. Happens when |location_in_screen| is within
  // kReturnToMaximizedThreshold threshold.
  bool ShouldRestoreToOriginalBounds(
      const gfx::Point& location_in_screen) const;

  // Returns true if we should go to home screen after drag ends. Happens when
  // the upward vertical velocity is larger than kVelocityToHomeScreenThreshold
  // and splitview is not active. Note when splitview is active, we do not allow
  // to go to home screen by fling.
  bool ShouldGoToHomeScreen(base::Optional<float> velocity_y) const;

  // Returns the desired snap position on |location_in_screen| when drag ends.
  SplitViewController::SnapPosition GetSnapPositionOnDragEnd(
      const gfx::Point& location_in_screen,
      base::Optional<float> velocity_y) const;

  // Returns true if we should drop the dragged window in overview after drag
  // ends.
  bool ShouldDropWindowInOverview(const gfx::Point& location_in_screen,
                                  base::Optional<float> velocity_y) const;

  // Reshows the windows that were hidden before drag starts.
  void ReshowHiddenWindowsOnDragEnd();

  aura::Window* const window_;
  gfx::Point initial_location_in_screen_;
  gfx::Point previous_location_in_screen_;
  bool drag_started_ = false;
  BackdropWindowMode original_backdrop_mode_ = BackdropWindowMode::kAuto;

  // The windows that are hidden during window dragging. Depends on different
  // scenarios, we may or may not reshow there windows when drag ends.
  std::vector<aura::Window*> hidden_windows_;

  DISALLOW_COPY_AND_ASSIGN(DragWindowFromShelfController);
};

}  // namespace ash

#endif  // ASH_HOME_SCREEN_DRAG_WINDOW_FROM_SHELF_CONTROLLER_H_
