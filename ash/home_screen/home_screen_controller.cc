// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_controller.h"

#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/display/manager/display_manager.h"

namespace ash {
namespace {

// Minimizes all windows that aren't in the home screen container. Done in
// reverse order to preserve the mru ordering.
// Returns true if any windows are minimized.
bool MinimizeAllWindows() {
  bool handled = false;
  aura::Window* container = Shell::Get()->GetPrimaryRootWindow()->GetChildById(
      kShellWindowId_HomeScreenContainer);
  // The home screen opens for the current active desk, there's no need to
  // minimize windows in the inactive desks.
  aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  for (auto it = windows.rbegin(); it != windows.rend(); it++) {
    if (!container->Contains(*it) && !WindowState::Get(*it)->IsMinimized()) {
      WindowState::Get(*it)->Minimize();
      handled = true;
    }
  }
  return handled;
}

}  // namespace

HomeScreenController::HomeScreenController()
    : home_launcher_gesture_handler_(
          std::make_unique<HomeLauncherGestureHandler>()) {
  Shell::Get()->overview_controller()->AddObserver(this);
  Shell::Get()->wallpaper_controller()->AddObserver(this);
}

HomeScreenController::~HomeScreenController() {
  Shell::Get()->wallpaper_controller()->RemoveObserver(this);
  Shell::Get()->overview_controller()->RemoveObserver(this);
}

void HomeScreenController::Show() {
  DCHECK(Shell::Get()->tablet_mode_controller()->InTabletMode());

  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return;

  delegate_->ShowHomeScreenView();
  UpdateVisibility();

  aura::Window* window = delegate_->GetHomeScreenWindow();
  if (window)
    Shelf::ForWindow(window)->MaybeUpdateShelfBackground();
}

bool HomeScreenController::GoHome(int64_t display_id) {
  DCHECK(Shell::Get()->tablet_mode_controller()->InTabletMode());

  if (home_launcher_gesture_handler_->ShowHomeLauncher(
          Shell::Get()->display_manager()->GetDisplayForId(display_id))) {
    return true;
  }

  if (Shell::Get()->overview_controller()->InOverviewSession()) {
    // End overview mode.
    Shell::Get()->overview_controller()->EndOverview(
        OverviewSession::EnterExitOverviewType::kSlideOutExit);
    return true;
  }

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  if (split_view_controller->InSplitViewMode()) {
    // End split view mode.
    split_view_controller->EndSplitView(
        SplitViewController::EndReason::kHomeLauncherPressed);
    return true;
  }

  if (MinimizeAllWindows())
    return true;

  return false;
}

void HomeScreenController::SetDelegate(HomeScreenDelegate* delegate) {
  delegate_ = delegate;
}

void HomeScreenController::OnWindowDragStarted() {
  in_window_dragging_ = true;
  UpdateVisibility();
}

void HomeScreenController::OnWindowDragEnded() {
  in_window_dragging_ = false;
  UpdateVisibility();
}

bool HomeScreenController::IsHomeScreenVisible() const {
  return delegate_->IsHomeScreenVisible();
}

void HomeScreenController::OnOverviewModeStarting() {
  const OverviewSession::EnterExitOverviewType overview_enter_type =
      Shell::Get()
          ->overview_controller()
          ->overview_session()
          ->enter_exit_overview_type();

  const bool animate =
      overview_enter_type ==
          OverviewSession::EnterExitOverviewType::kSlideInEnter ||
      overview_enter_type ==
          OverviewSession::EnterExitOverviewType::kFadeInEnter;
  const HomeScreenPresenter::TransitionType transition =
      overview_enter_type ==
              OverviewSession::EnterExitOverviewType::kFadeInEnter
          ? HomeScreenPresenter::TransitionType::kScaleHomeOut
          : HomeScreenPresenter::TransitionType::kSlideHomeOut;

  home_screen_presenter_.ScheduleOverviewModeAnimation(transition, animate);
}

void HomeScreenController::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  // The launcher should be shown after overview mode finishes animating, in
  // OnOverviewModeEndingAnimationComplete(). Overview however is nullptr by the
  // time the animations are finished, so cache the exit type here.
  overview_exit_type_ =
      base::make_optional(overview_session->enter_exit_overview_type());
}

void HomeScreenController::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  if (canceled)
    return;
  DCHECK(overview_exit_type_.has_value());

  const bool animate =
      *overview_exit_type_ ==
          OverviewSession::EnterExitOverviewType::kSlideOutExit ||
      *overview_exit_type_ ==
          OverviewSession::EnterExitOverviewType::kFadeOutExit;
  const HomeScreenPresenter::TransitionType transition =
      *overview_exit_type_ ==
              OverviewSession::EnterExitOverviewType::kFadeOutExit
          ? HomeScreenPresenter::TransitionType::kScaleHomeIn
          : HomeScreenPresenter::TransitionType::kSlideHomeIn;
  overview_exit_type_ = base::nullopt;

  home_screen_presenter_.ScheduleOverviewModeAnimation(transition, animate);
}

void HomeScreenController::OnWallpaperPreviewStarted() {
  in_wallpaper_preview_ = true;
  UpdateVisibility();
}

void HomeScreenController::OnWallpaperPreviewEnded() {
  in_wallpaper_preview_ = false;
  UpdateVisibility();
}

void HomeScreenController::UpdateVisibility() {
  if (!Shell::Get()->tablet_mode_controller()->InTabletMode())
    return;

  aura::Window* window = delegate_->GetHomeScreenWindow();
  if (!window)
    return;

  const bool in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();
  if (in_overview || in_wallpaper_preview_ || in_window_dragging_)
    window->Hide();
  else
    window->Show();
}

}  // namespace ash
