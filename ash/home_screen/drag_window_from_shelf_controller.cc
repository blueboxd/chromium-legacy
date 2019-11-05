// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/drag_window_from_shelf_controller.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/home_screen/window_transform_to_home_screen_animation.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/scoped_animation_disabler.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "base/numerics/ranges.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/transform_util.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The minimum window scale factor when dragging a window from shelf.
constexpr float kMinimumWindowScaleDuringDragging = 0.2f;

// Amount of time to wait to show overview after the user slows down or stops
// window dragging.
constexpr base::TimeDelta kShowOverviewTimeWhenDragSuspend =
    base::TimeDelta::FromMilliseconds(40);

}  // namespace

// Hide all visible windows expect the dragged windows or the window showing in
// splitview during dragging.
class DragWindowFromShelfController::WindowsHider
    : public aura::WindowObserver {
 public:
  explicit WindowsHider(aura::Window* dragged_window)
      : dragged_window_(dragged_window) {
    std::vector<aura::Window*> windows =
        Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
    for (auto* window : windows) {
      if (window == dragged_window_)
        continue;
      if (!window->IsVisible())
        continue;
      if (SplitViewController::Get(window)->IsWindowInSplitView(window))
        continue;

      hidden_windows_.push_back(window);
      {
        ScopedAnimationDisabler disabler(window);
        // Minimize so that they can show up correctly in overview.
        WindowState::Get(window)->Minimize();
        window->Hide();
      }
      window->AddObserver(this);
    }
  }

  ~WindowsHider() override {
    for (auto* window : hidden_windows_)
      window->RemoveObserver(this);
    hidden_windows_.clear();
  }

  void RestoreWindowsVisibility() {
    for (auto* window : hidden_windows_) {
      window->RemoveObserver(this);
      ScopedAnimationDisabler disabler(window);
      window->Show();
    }
    hidden_windows_.clear();
  }

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override {
    window->RemoveObserver(this);
    hidden_windows_.erase(
        std::find(hidden_windows_.begin(), hidden_windows_.end(), window));
  }

 private:
  aura::Window* dragged_window_;
  std::vector<aura::Window*> hidden_windows_;

  DISALLOW_COPY_AND_ASSIGN(WindowsHider);
};

DragWindowFromShelfController::DragWindowFromShelfController(
    aura::Window* window)
    : window_(window) {
  window_->AddObserver(this);
}

DragWindowFromShelfController::~DragWindowFromShelfController() {
  CancelDrag();
  if (window_)
    window_->RemoveObserver(this);
}

void DragWindowFromShelfController::Drag(const gfx::Point& location_in_screen,
                                         float scroll_x,
                                         float scroll_y) {
  // |window_| might have been destroyed during dragging.
  if (!window_)
    return;

  if (!drag_started_) {
    // Do not start drag until the drag goes above the hotseat.
    const gfx::Rect work_area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            window_);
    if (location_in_screen.y() >
        work_area.bottom() - ShelfConfig::Get()->hotseat_size()) {
      return;
    }
    OnDragStarted(location_in_screen);
  }

  UpdateDraggedWindow(location_in_screen);

  // Open overview if the window has been dragged far enough and the scroll
  // delta has decreased to kOpenOverviewThreshold or less.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (std::abs(scroll_y) <= kOpenOverviewThreshold &&
      !overview_controller->InOverviewSession()) {
    overview_controller->StartOverview(
        OverviewSession::EnterExitOverviewType::kImmediateEnter);
    OverviewSession* overview_session = overview_controller->overview_session();
    overview_session->OnWindowDragStarted(window_, /*animate=*/false);
    if (ShouldAllowSplitView())
      overview_session->SetSplitViewDragIndicatorsDraggedWindow(window_);
  }

  // If overview is active, update its splitview indicator during dragging if
  // splitview is allowed in current configuration.
  if (overview_controller->InOverviewSession()) {
    const SplitViewController::SnapPosition snap_position =
        GetSnapPosition(location_in_screen);
    // Pass non_snap_state=kNoDrag so that only snap previews show up.
    // TODO(crbug.com/1020349): Achieve that effect another way, because this
    // way causes incorrect animations.
    SplitViewDragIndicators::WindowDraggingState window_dragging_state =
        SplitViewDragIndicators::ComputeWindowDraggingState(
            drag_started_,
            SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            snap_position);
    OverviewSession* overview_session = overview_controller->overview_session();
    overview_session->SetSplitViewDragIndicatorsWindowDraggingState(
        window_dragging_state, location_in_screen);
    overview_session->OnWindowDragContinued(
        window_, gfx::PointF(location_in_screen), window_dragging_state);

    if (snap_position != SplitViewController::NONE) {
      // If the dragged window is in snap preview area, make sure overview is
      // visible.
      ShowOverviewDuringOrAfterDrag();
    } else if (std::abs(scroll_x) > kShowOverviewThreshold ||
               std::abs(scroll_y) > kShowOverviewThreshold) {
      // If the dragging velocity is large enough, hide overview windows.
      show_overview_timer_.Stop();
      overview_session->SetVisibleDuringWindowDragging(/*visible=*/false);
    } else {
      // Otherwise start the |show_overview_timer_| to show and update overview
      // when the dragging slows down or stops.
      show_overview_timer_.Start(
          FROM_HERE, kShowOverviewTimeWhenDragSuspend, this,
          &DragWindowFromShelfController::ShowOverviewDuringOrAfterDrag);
    }
  }

  previous_location_in_screen_ = location_in_screen;
}

void DragWindowFromShelfController::EndDrag(
    const gfx::Point& location_in_screen,
    base::Optional<float> velocity_y) {
  if (!drag_started_)
    return;

  drag_started_ = false;
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  const bool in_overview = overview_controller->InOverviewSession();
  const bool in_splitview = split_view_controller->InSplitViewMode();

  if (ShouldGoToHomeScreen(velocity_y)) {
    DCHECK(!in_splitview);
    if (in_overview) {
      overview_controller->EndOverview(
          OverviewSession::EnterExitOverviewType::kImmediateExit);
    }
    ScaleDownWindowAfterDrag();
  } else if (ShouldRestoreToOriginalBounds(location_in_screen)) {
    // TODO(crbug.com/997885): Add animation.
    SetTransform(window_, gfx::Transform());
    window_->SetProperty(kBackdropWindowMode, original_backdrop_mode_);
    if (!in_splitview && in_overview) {
      overview_controller->EndOverview(
          OverviewSession::EnterExitOverviewType::kImmediateExit);
    }
    ReshowHiddenWindowsOnDragEnd();
  } else if (!in_overview) {
    // if overview is not active during the entire drag process, scale down the
    // dragged window to go to home screen.
    ScaleDownWindowAfterDrag();
  }

  OnDragEnded(location_in_screen,
              ShouldDropWindowInOverview(location_in_screen, velocity_y),
              GetSnapPositionOnDragEnd(location_in_screen, velocity_y));
}

void DragWindowFromShelfController::CancelDrag() {
  if (!drag_started_)
    return;

  drag_started_ = false;
  // Reset the window's transform to identity transform.
  window_->SetTransform(gfx::Transform());
  window_->SetProperty(kBackdropWindowMode, original_backdrop_mode_);

  // End overview if it was opened during dragging.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    overview_controller->EndOverview(
        OverviewSession::EnterExitOverviewType::kImmediateExit);
  }
  ReshowHiddenWindowsOnDragEnd();

  OnDragEnded(previous_location_in_screen_,
              /*should_drop_window_in_overview=*/false,
              /*snap_position=*/SplitViewController::NONE);
}

void DragWindowFromShelfController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);

  CancelDrag();
  window_->RemoveObserver(this);
  window_ = nullptr;
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

  // Hide all visible windows behind the dragged window during dragging.
  windows_hider_ = std::make_unique<WindowsHider>(window_);

  // Hide the home launcher until it's eligible to show it.
  Shell::Get()->home_screen_controller()->OnWindowDragStarted();

  // Use the same dim and blur as in overview during dragging.
  auto* wallpaper_view =
      RootWindowController::ForWindow(window_->GetRootWindow())
          ->wallpaper_widget_controller()
          ->wallpaper_view();
  if (wallpaper_view) {
    wallpaper_view->RepaintBlurAndOpacity(kWallpaperBlurSigma, kShieldOpacity);
  }

  // If the dragged window is one of the snapped window in splitview, it needs
  // to be detached from splitview before start dragging.
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->OnWindowDragStarted(window_);
  // Note SplitViewController::OnWindowDragStarted() may open overview.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    OverviewSession* overview_session = overview_controller->overview_session();
    overview_session->OnWindowDragStarted(window_, /*animate=*/false);
    if (ShouldAllowSplitView())
      overview_session->SetSplitViewDragIndicatorsDraggedWindow(window_);
  }
}

void DragWindowFromShelfController::OnDragEnded(
    const gfx::Point& location_in_screen,
    bool should_drop_window_in_overview,
    SplitViewController::SnapPosition snap_position) {
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (overview_controller->InOverviewSession()) {
    // Make sure overview is visible after drag ends.
    ShowOverviewDuringOrAfterDrag();

    OverviewSession* overview_session = overview_controller->overview_session();
    overview_session->SetSplitViewDragIndicatorsWindowDraggingState(
        SplitViewDragIndicators::WindowDraggingState::kNoDrag,
        location_in_screen);
    overview_session->OnWindowDragEnded(
        window_, gfx::PointF(location_in_screen),
        should_drop_window_in_overview,
        /*snap=*/snap_position != SplitViewController::NONE);
  }

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  if (split_view_controller->InSplitViewMode() ||
      snap_position != SplitViewController::NONE) {
    split_view_controller->OnWindowDragEnded(window_, snap_position,
                                             location_in_screen);
  }

  Shell::Get()->home_screen_controller()->OnWindowDragEnded();

  // Clear the wallpaper dim and blur if not in overview after drag ends.
  // If in overview, the dim and blur will be cleared after overview ends.
  if (!overview_controller->InOverviewSession()) {
    auto* wallpaper_view =
        RootWindowController::ForWindow(window_->GetRootWindow())
            ->wallpaper_widget_controller()
            ->wallpaper_view();
    if (wallpaper_view)
      wallpaper_view->RepaintBlurAndOpacity(kWallpaperClearBlurSigma, 1.f);
  }

  WindowState::Get(window_)->DeleteDragDetails();
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

  // The dragged window cannot exceed the top of the display. So calculate the
  // expected transformed bounds and then adjust the transform if needed.
  gfx::RectF transformed_bounds(window_->bounds());
  gfx::Transform new_tranform = TransformAboutPivot(
      gfx::ToRoundedPoint(transformed_bounds.origin()), transform);
  new_tranform.TransformRect(&transformed_bounds);
  ::wm::TranslateRectToScreen(window_->parent(), &transformed_bounds);
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()
          ->GetDisplayNearestPoint(location_in_screen)
          .bounds();
  if (transformed_bounds.y() < display_bounds.y()) {
    transform.Translate(0,
                        (display_bounds.y() - transformed_bounds.y()) / scale);
  }

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
         !SplitViewController::Get(Shell::GetPrimaryRootWindow())
              ->InSplitViewMode();
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
      SplitViewController::Get(Shell::GetPrimaryRootWindow())
          ->InSplitViewMode();
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
  windows_hider_->RestoreWindowsVisibility();
}

void DragWindowFromShelfController::ShowOverviewDuringOrAfterDrag() {
  show_overview_timer_.Stop();
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return;

  overview_controller->overview_session()->SetVisibleDuringWindowDragging(
      /*visible=*/true);
}

void DragWindowFromShelfController::ScaleDownWindowAfterDrag() {
  // Do the scale-down transform for the entire transient tree.
  for (auto* window : GetTransientTreeIterator(window_)) {
    // self-destructed when window transform animation is done.
    new WindowTransformToHomeScreenAnimation(
        window, window == window_ ? base::make_optional(original_backdrop_mode_)
                                  : base::nullopt);
  }
}

}  // namespace ash
