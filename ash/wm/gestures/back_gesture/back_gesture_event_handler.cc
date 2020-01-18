// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_event_handler.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/gestures/back_gesture/back_gesture_affordance.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/metrics/user_metrics.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Distance from the divider's center point that reserved for splitview
// resizing in landscape orientation.
constexpr int kDistanceForSplitViewResize = 49;

// Called by CanStartGoingBack() to check whether we can start swiping from the
// split view divider to go back.
bool CanStartGoingBackFromSplitViewDivider(const gfx::Point& screen_location) {
  if (!IsCurrentScreenOrientationLandscape())
    return false;

  auto* root_window = window_util::GetRootWindowAt(screen_location);
  auto* split_view_controller = SplitViewController::Get(root_window);
  if (!split_view_controller->InTabletSplitViewMode())
    return false;

  // Do not enable back gesture if |screen_location| is inside the extended
  // hotseat, let the hotseat handle the event instead.
  Shelf* shelf = Shelf::ForWindow(root_window);
  if (shelf->shelf_layout_manager()->hotseat_state() ==
          HotseatState::kExtended &&
      shelf->shelf_widget()
          ->hotseat_widget()
          ->GetWindowBoundsInScreen()
          .Contains(screen_location)) {
    return false;
  }

  // Do not enable back gesture if |screen_location| is inside the shelf widget,
  // let the shelf handle the event instead.
  if (shelf->shelf_widget()->GetWindowBoundsInScreen().Contains(
          screen_location)) {
    return false;
  }

  gfx::Rect divider_bounds =
      split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
          /*is_dragging=*/false);
  const int y_center = divider_bounds.CenterPoint().y();
  // Do not enable back gesture if swiping starts from splitview divider's
  // resizable area.
  if (screen_location.y() >= (y_center - kDistanceForSplitViewResize) &&
      screen_location.y() <= (y_center + kDistanceForSplitViewResize)) {
    return false;
  }

  divider_bounds.set_x(divider_bounds.x() -
                       SplitViewDivider::kDividerEdgeInsetForTouch);
  divider_bounds.set_width(
      divider_bounds.width() + SplitViewDivider::kDividerEdgeInsetForTouch +
      BackGestureEventHandler::kStartGoingBackLeftEdgeInset);
  return divider_bounds.Contains(screen_location);
}

// Activate the given |window|.
void ActivateWindow(aura::Window* window) {
  if (!window)
    return;
  WindowState::Get(window)->Activate();
}

// Activate the snapped window that is underneath the start |location| for back
// gesture. This is necessary since the snapped window that is underneath is not
// always the current active window.
void ActivateUnderneathWindowInSplitViewMode(
    const gfx::Point& location,
    bool dragged_from_splitview_divider) {
  auto* split_view_controller =
      SplitViewController::Get(window_util::GetRootWindowAt(location));
  if (!split_view_controller->InTabletSplitViewMode())
    return;

  auto* left_window = split_view_controller->left_window();
  auto* right_window = split_view_controller->right_window();
  const OrientationLockType current_orientation = GetCurrentScreenOrientation();
  if (current_orientation == OrientationLockType::kLandscapePrimary) {
    ActivateWindow(dragged_from_splitview_divider ? right_window : left_window);
  } else if (current_orientation == OrientationLockType::kLandscapeSecondary) {
    ActivateWindow(dragged_from_splitview_divider ? left_window : right_window);
  } else {
    if (left_window &&
        split_view_controller
            ->GetSnappedWindowBoundsInScreen(
                SplitViewController::LEFT, /*window_for_minimum_size=*/nullptr)
            .Contains(location)) {
      ActivateWindow(left_window);
    } else if (right_window && split_view_controller
                                   ->GetSnappedWindowBoundsInScreen(
                                       SplitViewController::RIGHT,
                                       /*window_for_minimum_size=*/nullptr)
                                   .Contains(location)) {
      ActivateWindow(right_window);
    } else if (split_view_controller->split_view_divider()
                   ->GetDividerBoundsInScreen(
                       /*is_dragging=*/false)
                   .Contains(location)) {
      // Activate the window that above the splitview divider if back gesture
      // starts from splitview divider.
      ActivateWindow(IsCurrentScreenOrientationPrimary() ? left_window
                                                         : right_window);
    }
  }
}

}  // namespace

BackGestureEventHandler::BackGestureEventHandler()
    : gesture_provider_(this, this) {
  display::Screen::GetScreen()->AddObserver(this);
}

BackGestureEventHandler::~BackGestureEventHandler() {
  display::Screen::GetScreen()->RemoveObserver(this);
}

void BackGestureEventHandler::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  // Cancel the left edge swipe back during screen rotation.
  if (metrics & DISPLAY_METRIC_ROTATION) {
    back_gesture_affordance_.reset();
    going_back_started_ = false;
  }
}

void BackGestureEventHandler::OnGestureEvent(ui::GestureEvent* event) {}

void BackGestureEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  if (first_touch_id_ == ui::kPointerIdUnknown)
    first_touch_id_ = event->pointer_details().id;

  if (event->pointer_details().id != first_touch_id_)
    return;

  if (event->type() == ui::ET_TOUCH_RELEASED)
    first_touch_id_ = ui::kPointerIdUnknown;

  if (event->type() == ui::ET_TOUCH_PRESSED) {
    x_drag_amount_ = y_drag_amount_ = 0;
    during_reverse_dragging_ = false;
  } else {
    // TODO(oshima): Convert to PointF/float.
    const gfx::Point current_location = event->location();
    x_drag_amount_ += (current_location.x() - last_touch_point_.x());
    y_drag_amount_ += (current_location.y() - last_touch_point_.y());

    // Do not update |during_reverse_dragging_| if touch point's location
    // doesn't change.
    if (current_location.x() < last_touch_point_.x())
      during_reverse_dragging_ = true;
    else if (current_location.x() > last_touch_point_.x())
      during_reverse_dragging_ = false;
  }
  last_touch_point_ = event->location();

  ui::TouchEvent touch_event_copy = *event;
  if (!gesture_provider_.OnTouchEvent(&touch_event_copy))
    return;

  gesture_provider_.OnTouchEventAck(
      touch_event_copy.unique_event_id(), /*event_consumed=*/false,
      /*is_source_touch_event_set_non_blocking=*/false);

  // Get the event target from TouchEvent since target of the GestureEvent from
  // GetAndResetPendingGestures is nullptr.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  const std::vector<std::unique_ptr<ui::GestureEvent>> gestures =
      gesture_provider_.GetAndResetPendingGestures();
  for (const auto& gesture : gestures) {
    if (MaybeHandleBackGesture(gesture.get(), target))
      event->StopPropagation();
  }
}

void BackGestureEventHandler::OnGestureEvent(GestureConsumer* consumer,
                                             ui::GestureEvent* event) {
  // Gesture events here are generated by |gesture_provider_|, and they're
  // handled at OnTouchEvent() by calling MaybeHandleBackGesture().
}

bool BackGestureEventHandler::MaybeHandleBackGesture(ui::GestureEvent* event,
                                                     aura::Window* target) {
  DCHECK(features::IsSwipingFromLeftEdgeToGoBackEnabled());
  DCHECK(target);
  gfx::Point screen_location = event->location();
  ::wm::ConvertPointToScreen(target, &screen_location);
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      going_back_started_ = CanStartGoingBack(target, screen_location);
      if (!going_back_started_)
        break;
      back_gesture_affordance_ = std::make_unique<BackGestureAffordance>(
          screen_location, dragged_from_splitview_divider_);
      return true;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (!going_back_started_)
        break;
      back_start_location_ = screen_location;

      base::RecordAction(base::UserMetricsAction("Ash_Tablet_BackGesture"));
      back_gesture_start_scenario_type_ = GetStartScenarioType(
          dragged_from_splitview_divider_, back_start_location_);
      RecordStartScenarioType(back_gesture_start_scenario_type_);
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (!going_back_started_)
        break;
      DCHECK(back_gesture_affordance_);
      back_gesture_affordance_->Update(x_drag_amount_, y_drag_amount_,
                                       during_reverse_dragging_);
      return true;
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START: {
      if (!going_back_started_)
        break;
      DCHECK(back_gesture_affordance_);
      // Complete the back gesture if the affordance is activated or fling with
      // large enough velocity. Note, complete can be different actions while in
      // different scenarios, but always fading out the affordance at the end.
      if (back_gesture_affordance_->IsActivated() ||
          (event->type() == ui::ET_SCROLL_FLING_START &&
           event->details().velocity_x() >= kFlingVelocityForGoingBack)) {
        ActivateUnderneathWindowInSplitViewMode(
            back_start_location_, dragged_from_splitview_divider_);
        auto* top_window_state =
            WindowState::Get(TabletModeWindowManager::GetTopWindow());
        if (top_window_state && top_window_state->IsFullscreen() &&
            !Shell::Get()->overview_controller()->InOverviewSession()) {
          // Complete as exiting the fullscreen mode of the underneath window.
          const WMEvent event(WM_EVENT_TOGGLE_FULLSCREEN);
          top_window_state->OnWMEvent(&event);
          RecordEndScenarioType(BackGestureEndScenarioType::kExitFullscreen);
        } else if (TabletModeWindowManager::ShouldMinimizeTopWindowOnBack()) {
          // Complete as minimizing the underneath window.
          top_window_state->Minimize();
          RecordEndScenarioType(
              GetEndScenarioType(back_gesture_start_scenario_type_,
                                 BackGestureEndType::kMinimize));
        } else {
          // Complete as going back to the previous page of the underneath
          // window.
          aura::Window* root_window =
              window_util::GetRootWindowAt(screen_location);
          ui::KeyEvent press_key_event(ui::ET_KEY_PRESSED,
                                       ui::VKEY_BROWSER_BACK, ui::EF_NONE);
          ignore_result(
              root_window->GetHost()->SendEventToSink(&press_key_event));
          ui::KeyEvent release_key_event(ui::ET_KEY_RELEASED,
                                         ui::VKEY_BROWSER_BACK, ui::EF_NONE);
          ignore_result(
              root_window->GetHost()->SendEventToSink(&release_key_event));
          RecordEndScenarioType(GetEndScenarioType(
              back_gesture_start_scenario_type_, BackGestureEndType::kBack));
        }
        back_gesture_affordance_->Complete();
      } else {
        back_gesture_affordance_->Abort();
        RecordEndScenarioType(GetEndScenarioType(
            back_gesture_start_scenario_type_, BackGestureEndType::kAbort));
      }
      RecordUnderneathWindowType(
          GetUnderneathWindowType(back_gesture_start_scenario_type_));
      return true;
    }
    case ui::ET_GESTURE_END:
      going_back_started_ = false;
      dragged_from_splitview_divider_ = false;
      break;
    default:
      break;
  }

  return going_back_started_;
}

bool BackGestureEventHandler::CanStartGoingBack(
    aura::Window* target,
    const gfx::Point& screen_location) {
  DCHECK(features::IsSwipingFromLeftEdgeToGoBackEnabled());

  Shell* shell = Shell::Get();
  if (!shell->tablet_mode_controller()->InTabletMode())
    return false;

  // Do not enable back gesture if it is not in an ACTIVE session. e.g, login
  // screen, lock screen.
  if (shell->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return false;
  }

  // Do not enable back gesture if home screen is visible but not in
  // |kFullscreenSearch| state.
  if (shell->home_screen_controller()->IsHomeScreenVisible() &&
      shell->app_list_controller()->GetAppListViewState() !=
          AppListViewState::kFullscreenSearch) {
    return false;
  }

  // Do not enable back gesture if MRU window list is empty and it is not in
  // overview mode.
  if (!Shell::Get()->overview_controller()->InOverviewSession() &&
      !TabletModeWindowManager::GetTopWindow()) {
    return false;
  }

  gfx::Rect hit_bounds_in_screen(display::Screen::GetScreen()
                                     ->GetDisplayNearestWindow(target)
                                     .work_area());
  hit_bounds_in_screen.set_width(kStartGoingBackLeftEdgeInset);
  if (hit_bounds_in_screen.Contains(screen_location))
    return true;

  dragged_from_splitview_divider_ =
      CanStartGoingBackFromSplitViewDivider(screen_location);
  return dragged_from_splitview_divider_;
}

}  // namespace ash
