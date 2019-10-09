// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/drag_window_from_shelf_controller.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_state.h"
#include "base/numerics/ranges.h"
#include "ui/base/hit_test.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The minimum window scale factor when dragging a window from shelf.
constexpr float kMinimumWindowScaleDuringDragging = 0.2f;

}  // namespace

DragWindowFromShelfController::DragWindowFromShelfController(
    aura::Window* window,
    const std::vector<aura::Window*>& hidden_windows)
    : window_(window), hidden_windows_(hidden_windows) {}

DragWindowFromShelfController::~DragWindowFromShelfController() {
  CancelDrag();
}

void DragWindowFromShelfController::Drag(const gfx::Point& location_in_screen,
                                         float scroll_y) {
  if (!drag_started_) {
    // Do not start drag until the drag goes above the shelf.
    const gfx::Rect work_area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            window_);
    if (location_in_screen.y() > work_area.bottom())
      return;
    OnDragStarted(location_in_screen);
  }
  UpdateDraggedWindow(location_in_screen);

  // Open overview if the window has been dragged far enough and the scroll
  // delta has decreased to kShowOverviewThreshold or less.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (std::abs(scroll_y) <= kShowOverviewThreshold &&
      !overview_controller->InOverviewSession()) {
    overview_controller->StartOverview(
        OverviewSession::EnterExitOverviewType::kImmediateEnter);
    overview_controller->overview_session()->OnWindowDragStarted(
        window_, /*animate=*/false);
  }

  // If overview is active, update its splitview indicator during dragging if
  // splitview is allowed in current configuration.
  if (overview_controller->InOverviewSession() && ShouldAllowSplitView()) {
    OverviewSession* overview_session = overview_controller->overview_session();
    IndicatorState indicator_state = GetIndicatorState(location_in_screen);
    overview_session->SetSplitViewDragIndicatorsIndicatorState(
        indicator_state, location_in_screen);
    overview_session->OnWindowDragContinued(
        window_, gfx::PointF(location_in_screen), indicator_state);
  }

  previous_location_in_screen_ = location_in_screen;
}

void DragWindowFromShelfController::EndDrag(
    const gfx::Point& location_in_screen,
    base::Optional<float> velocity_y) {
  if (!drag_started_) {
    ReshowHiddenWindowsOnDragEnd();
    return;
  }

  drag_started_ = false;
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  const bool in_overview = overview_controller->InOverviewSession();
  const bool in_splitview = split_view_controller->InSplitViewMode();

  if (ShouldGoToHomeScreen(velocity_y)) {
    DCHECK(!in_splitview);
    if (in_overview) {
      overview_controller->EndOverview(
          OverviewSession::EnterExitOverviewType::kImmediateExit);
    }
    // TODO(crbug.com/997885): Add animation.
    WindowState::Get(window_)->Minimize();
  } else if (ShouldRestoreToOriginalBounds(location_in_screen)) {
    // TODO(crbug.com/997885): Add animation.
    SetTransform(window_, gfx::Transform());
    if (!in_splitview && in_overview) {
      overview_controller->EndOverview(
          OverviewSession::EnterExitOverviewType::kImmediateExit);
    }
    ReshowHiddenWindowsOnDragEnd();
  } else if (!in_overview) {
    // if overview is not active during the entire drag process, go to home
    // screen.
    WindowState::Get(window_)->Minimize();
  }

  OnDragEnded(location_in_screen,
              ShouldDropWindowInOverview(location_in_screen, velocity_y),
              GetSnapPositionOnDragEnd(location_in_screen, velocity_y));
}

void DragWindowFromShelfController::CancelDrag() {
  if (!drag_started_) {
    ReshowHiddenWindowsOnDragEnd();
    return;
  }

  drag_started_ = false;
  // Reset the window's transform to identity transform.
  window_->SetTransform(gfx::Transform());
  ReshowHiddenWindowsOnDragEnd();

  OnDragEnded(previous_location_in_screen_,
              /*should_drop_window_in_overview=*/false,
              /*snap_position=*/SplitViewController::NONE);
}

void DragWindowFromShelfController::OnDragStarted(
    const gfx::Point& location_in_screen) {
  drag_started_ = true;
  initial_location_in_screen_ = location_in_screen;
  previous_location_in_screen_ = location_in_screen;
  WindowState::Get(window_)->CreateDragDetails(
      initial_location_in_screen_, HTCLIENT, ::wm::WINDOW_MOVE_SOURCE_TOUCH);

  // Disable the backdrop on the dragged window during dragging.
  original_backdrop_mode_ = window_->GetProperty(kBackdropWindowMode);
  window_->SetProperty(kBackdropWindowMode, BackdropWindowMode::kDisabled);

  // Hide the home launcher until it's eligible to show it.
  Shell::Get()->home_screen_controller()->OnWindowDragStarted();

  // If the dragged window is one of the snapped window in splitview, it needs
  // to be detached from splitview before start dragging.
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  split_view_controller->OnWindowDragStarted(window_);
  // Note SplitViewController::OnWindowDragStarted() may open overview.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    overview_controller->overview_session()->OnWindowDragStarted(
        window_, /*animate=*/false);
  }
}

void DragWindowFromShelfController::OnDragEnded(
    const gfx::Point& location_in_screen,
    bool should_drop_window_in_overview,
    SplitViewController::SnapPosition snap_position) {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    OverviewSession* overview_session = overview_controller->overview_session();
    overview_session->SetSplitViewDragIndicatorsIndicatorState(
        IndicatorState::kNone, location_in_screen);
    overview_session->OnWindowDragEnded(
        window_, gfx::PointF(location_in_screen),
        should_drop_window_in_overview,
        /*snap=*/snap_position != SplitViewController::NONE);
  }
  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  if (split_view_controller->InSplitViewMode() ||
      snap_position != SplitViewController::NONE) {
    Shell::Get()->split_view_controller()->OnWindowDragEnded(
        window_, snap_position, location_in_screen);
  }
  Shell::Get()->home_screen_controller()->OnWindowDragEnded();

  WindowState::Get(window_)->DeleteDragDetails();
  window_->SetProperty(kBackdropWindowMode, original_backdrop_mode_);
  hidden_windows_.clear();
}

void DragWindowFromShelfController::UpdateDraggedWindow(
    const gfx::Point& location_in_screen) {
  gfx::Rect bounds = window_->bounds();
  ::wm::ConvertRectToScreen(window_->parent(), &bounds);

  // Calculate the window's transform based on the location.
  // For scale, at |initial_location_in_screen_|, the scale is 1.0, and at the
  // middle y position of its bounds, it reaches to its minimum scale 0.2.
  // Calculate the desired scale based on the current y position.
  int y_full = bounds.bottom() - bounds.CenterPoint().y();
  int y_diff = location_in_screen.y() - bounds.CenterPoint().y();
  float scale = (1.0f - kMinimumWindowScaleDuringDragging) * y_diff / y_full +
                kMinimumWindowScaleDuringDragging;
  scale = base::ClampToRange(scale, /*min=*/kMinimumWindowScaleDuringDragging,
                             /*max=*/1.f);

  // Calculate the desired translation so that the dragged window stays under
  // the finger during the dragging.
  gfx::Transform transform;
  transform.Translate(
      (location_in_screen.x() - bounds.x()) -
          (initial_location_in_screen_.x() - bounds.x()) * scale,
      (location_in_screen.y() - bounds.y()) -
          (initial_location_in_screen_.y() - bounds.y()) * scale);
  transform.Scale(scale, scale);

  SetTransform(window_, transform);
}

SplitViewController::SnapPosition
DragWindowFromShelfController::GetSnapPosition(
    const gfx::Point& location_in_screen) const {
  // if |location_in_screen| is close to the bottom of the screen and is
  // inside of kReturnToMaximizedThreshold threshold, we should not try to
  // snap the window.
  if (ShouldRestoreToOriginalBounds(location_in_screen))
    return SplitViewController::NONE;

  const gfx::Rect work_area = display::Screen::GetScreen()
                                  ->GetDisplayNearestPoint(location_in_screen)
                                  .work_area();
  SplitViewController::SnapPosition snap_position =
      ::ash::GetSnapPosition(window_, location_in_screen, work_area);

  // For portrait mode, since the drag starts from the bottom of the screen,
  // we should only allow the window to snap to the top of the screen.
  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  const bool is_primary = IsCurrentScreenOrientationPrimary();
  if (!is_landscape &&
      ((is_primary && snap_position == SplitViewController::RIGHT) ||
       (!is_primary && snap_position == SplitViewController::LEFT))) {
    snap_position = SplitViewController::NONE;
  }

  return snap_position;
}

IndicatorState DragWindowFromShelfController::GetIndicatorState(
    const gfx::Point& location_in_screen) const {
  if (!drag_started_)
    return IndicatorState::kNone;

  // if |location_in_screen| is close to the bottom of the screen and is
  // inside of kReturnToMaximizedThreshold threshold, we do not show the
  // indicators.
  if (ShouldRestoreToOriginalBounds(location_in_screen))
    return IndicatorState::kNone;

  IndicatorState indicator_state =
      ::ash::GetIndicatorState(window_, GetSnapPosition(location_in_screen));
  // For portrait mode, since the drag starts from the bottom of the
  // there is no bottom drag indicator.
  if (!IsCurrentScreenOrientationLandscape()) {
    if (indicator_state == IndicatorState::kDragArea)
      indicator_state = IndicatorState::kDragAreaLeft;
    else if (indicator_state == IndicatorState::kCannotSnap)
      indicator_state = IndicatorState::kCannotSnapLeft;
  }

  return indicator_state;
}

bool DragWindowFromShelfController::ShouldRestoreToOriginalBounds(
    const gfx::Point& location_in_screen) const {
  const gfx::Rect work_area = display::Screen::GetScreen()
                                  ->GetDisplayNearestPoint(location_in_screen)
                                  .work_area();
  return location_in_screen.y() >
         work_area.bottom() - kReturnToMaximizedThreshold;
}

bool DragWindowFromShelfController::ShouldGoToHomeScreen(
    base::Optional<float> velocity_y) const {
  return velocity_y.has_value() && *velocity_y < 0 &&
         std::abs(*velocity_y) >= kVelocityToHomeScreenThreshold &&
         !Shell::Get()->split_view_controller()->InSplitViewMode();
}

SplitViewController::SnapPosition
DragWindowFromShelfController::GetSnapPositionOnDragEnd(
    const gfx::Point& location_in_screen,
    base::Optional<float> velocity_y) const {
  if (ShouldRestoreToOriginalBounds(location_in_screen) ||
      ShouldGoToHomeScreen(velocity_y)) {
    return SplitViewController::NONE;
  }

  return GetSnapPosition(location_in_screen);
}

bool DragWindowFromShelfController::ShouldDropWindowInOverview(
    const gfx::Point& location_in_screen,
    base::Optional<float> velocity_y) const {
  if (!Shell::Get()->overview_controller()->InOverviewSession())
    return false;

  if (ShouldGoToHomeScreen(velocity_y))
    return false;

  const bool in_splitview =
      Shell::Get()->split_view_controller()->InSplitViewMode();
  if (!in_splitview && ShouldRestoreToOriginalBounds(location_in_screen)) {
    return false;
  }

  if (in_splitview) {
    if (velocity_y.has_value() && *velocity_y < 0 &&
        std::abs(*velocity_y) >= kVelocityToOverviewThreshold) {
      return true;
    }
    if (ShouldRestoreToOriginalBounds(location_in_screen))
      return false;
  }

  return GetSnapPositionOnDragEnd(location_in_screen, velocity_y) ==
         SplitViewController::NONE;
}

void DragWindowFromShelfController::ReshowHiddenWindowsOnDragEnd() {
  for (auto* window : hidden_windows_) {
    ScopedAnimationDisabler disable(window);
    window->Show();
  }
  hidden_windows_.clear();
}

}  // namespace ash
