// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/animation/animation_change_type.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/home_screen/home_screen_delegate.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/rotator/screen_rotation_animator.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/locale/locale_update_controller_impl.h"
#include "ash/system/status_area_widget.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_types.h"
#include "ash/wm/workspace_controller.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Delay before showing the shelf. This is after the mouse stops moving.
constexpr int kAutoHideDelayMS = 200;

// Duration of the animation to show or hide the shelf.
constexpr int kAnimationDurationMS = 200;

// To avoid hiding the shelf when the mouse transitions from a message bubble
// into the shelf, the hit test area is enlarged by this amount of pixels to
// keep the shelf from hiding.
constexpr int kNotificationBubbleGapHeight = 6;

// The maximum size of the region on the display opposing the shelf managed by
// this ShelfLayoutManager which can trigger showing the shelf.
// For instance:
// - Primary display is left of secondary display.
// - Shelf is left aligned
// - This ShelfLayoutManager manages the shelf for the secondary display.
// |kMaxAutoHideShowShelfRegionSize| refers to the maximum size of the region
// from the right edge of the primary display which can trigger showing the
// auto hidden shelf. The region is used to make it easier to trigger showing
// the auto hidden shelf when the shelf is on the boundary between displays.
constexpr int kMaxAutoHideShowShelfRegionSize = 10;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

// Returns true if the window is in the app list window container.
bool IsAppListWindow(const aura::Window* window) {
  const aura::Window* parent = window->parent();
  return parent && parent->id() == kShellWindowId_AppListContainer;
}

bool IsTabletModeEnabled() {
  // Shell could be destroying. Shell destroys TabletModeController before
  // closing all windows.
  if (!Shell::Get()->tablet_mode_controller())
    return false;

  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

bool IsHomeScreenAvailable() {
  // Shell could be destroying. Shell destroys HomeScreenController before
  // closing all windows.
  if (!Shell::Get()->home_screen_controller())
    return false;

  return Shell::Get()->home_screen_controller()->IsHomeScreenAvailable();
}

// Returns the |WorkspaceWindowState| of the currently active desk on the root
// window of |shelf_window|.
wm::WorkspaceWindowState GetShelfWorkspaceWindowState(
    aura::Window* shelf_window) {
  DCHECK(shelf_window);
  // Shelf window does not belong to any desk, use the root to get the active
  // desk's workspace state.
  auto* controller =
      GetActiveWorkspaceController(shelf_window->GetRootWindow());
  DCHECK(controller);

  return controller->GetWindowState();
}

// Returns shelf's work area inset for given |visibility_state| and |size|.
int GetShelfInset(ShelfVisibilityState visibility_state, int size) {
  return visibility_state == SHELF_VISIBLE ? size : 0;
}

// Sets the shelf opacity to 0 when the shelf is done hiding to avoid getting
// rid of blur.
class HideAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit HideAnimationObserver(ui::Layer* layer) : layer_(layer) {}

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsScheduled() override {}

  void OnImplicitAnimationsCompleted() override { layer_->SetOpacity(0); }

 private:
  // Unowned.
  ui::Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(HideAnimationObserver);
};

}  // namespace

// ShelfLayoutManager::UpdateShelfObserver -------------------------------------

// UpdateShelfObserver is used to delay updating the background until the
// animation completes.
class ShelfLayoutManager::UpdateShelfObserver
    : public ui::ImplicitAnimationObserver {
 public:
  explicit UpdateShelfObserver(ShelfLayoutManager* shelf) : shelf_(shelf) {
    shelf_->update_shelf_observer_ = this;
  }

  void Detach() { shelf_ = nullptr; }

  void OnImplicitAnimationsCompleted() override {
    if (shelf_)
      shelf_->MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
    delete this;
  }

 private:
  ~UpdateShelfObserver() override {
    if (shelf_)
      shelf_->update_shelf_observer_ = nullptr;
  }

  // Shelf we're in. nullptr if deleted before we're deleted.
  ShelfLayoutManager* shelf_;

  DISALLOW_COPY_AND_ASSIGN(UpdateShelfObserver);
};

ShelfLayoutManager::State::State()
    : visibility_state(SHELF_VISIBLE),
      auto_hide_state(SHELF_AUTO_HIDE_HIDDEN),
      window_state(wm::WORKSPACE_WINDOW_STATE_DEFAULT),
      pre_lock_screen_animation_active(false),
      session_state(session_manager::SessionState::UNKNOWN) {}

bool ShelfLayoutManager::State::IsAddingSecondaryUser() const {
  return session_state == session_manager::SessionState::LOGIN_SECONDARY;
}

bool ShelfLayoutManager::State::IsScreenLocked() const {
  return session_state == session_manager::SessionState::LOCKED;
}

bool ShelfLayoutManager::State::IsActiveSessionState() const {
  return session_state == session_manager::SessionState::ACTIVE;
}

bool ShelfLayoutManager::State::IsShelfAutoHidden() const {
  return visibility_state == SHELF_AUTO_HIDE &&
         auto_hide_state == SHELF_AUTO_HIDE_HIDDEN;
}

bool ShelfLayoutManager::State::IsShelfVisible() const {
  return visibility_state == SHELF_VISIBLE ||
         (visibility_state == SHELF_AUTO_HIDE &&
          auto_hide_state == SHELF_AUTO_HIDE_SHOWN);
}

bool ShelfLayoutManager::State::Equals(const State& other) const {
  return other.visibility_state == visibility_state &&
         (visibility_state != SHELF_AUTO_HIDE ||
          other.auto_hide_state == auto_hide_state) &&
         other.window_state == window_state &&
         other.pre_lock_screen_animation_active ==
             pre_lock_screen_animation_active &&
         other.session_state == session_state;
}

// ShelfLayoutManager ----------------------------------------------------------

ShelfLayoutManager::ShelfLayoutManager(ShelfWidget* shelf_widget, Shelf* shelf)
    : shelf_widget_(shelf_widget),
      shelf_(shelf),
      is_background_blur_enabled_(
          app_list_features::IsBackgroundBlurEnabled()) {
  DCHECK(shelf_widget_);
  DCHECK(shelf_);
}

ShelfLayoutManager::~ShelfLayoutManager() {
  if (update_shelf_observer_)
    update_shelf_observer_->Detach();

  for (auto& observer : observers_)
    observer.WillDeleteShelfLayoutManager();
  display::Screen::GetScreen()->RemoveObserver(this);
  Shell::Get()->locale_update_controller()->RemoveObserver(this);
  Shell::Get()->RemoveShellObserver(this);
  Shell::Get()->lock_state_controller()->RemoveObserver(this);
  // The following are destroyed early when Shell is being destroyed so they
  // may not exist.
  if (Shell::Get()->home_screen_controller()) {
    Shell::Get()
        ->home_screen_controller()
        ->home_launcher_gesture_handler()
        ->RemoveObserver(this);
  }
  if (Shell::Get()->app_list_controller())
    Shell::Get()->app_list_controller()->RemoveObserver(this);
  if (Shell::Get()->overview_controller())
    Shell::Get()->overview_controller()->RemoveObserver(this);
}

void ShelfLayoutManager::InitObservers() {
  Shell::Get()->AddShellObserver(this);
  Shell::Get()->overview_controller()->AddObserver(this);
  if (Shell::Get()->app_list_controller())
    Shell::Get()->app_list_controller()->AddObserver(this);
  Shell::Get()
      ->home_screen_controller()
      ->home_launcher_gesture_handler()
      ->AddObserver(this);
  Shell::Get()->lock_state_controller()->AddObserver(this);
  Shell::Get()->activation_client()->AddObserver(this);
  Shell::Get()->locale_update_controller()->AddObserver(this);
  state_.session_state = Shell::Get()->session_controller()->GetSessionState();
  shelf_background_type_ = GetShelfBackgroundType();
  wallpaper_controller_observer_.Add(Shell::Get()->wallpaper_controller());
  display::Screen::GetScreen()->AddObserver(this);
  ScreenRotationAnimator::GetForRootWindow(
      shelf_widget_->GetNativeWindow()->GetRootWindow())
      ->AddObserver(this);
}

void ShelfLayoutManager::PrepareForShutdown() {
  in_shutdown_ = true;

  // Stop observing changes to avoid updating a partially destructed shelf.
  Shell::Get()->activation_client()->RemoveObserver(this);
  ScreenRotationAnimator::GetForRootWindow(
      shelf_widget_->GetNativeWindow()->GetRootWindow())
      ->RemoveObserver(this);
}

bool ShelfLayoutManager::IsVisible() const {
  // status_area_widget() may be nullptr during the shutdown.
  return shelf_widget_->status_area_widget() &&
         shelf_widget_->status_area_widget()->IsVisible() &&
         state_.IsShelfVisible();
}

gfx::Rect ShelfLayoutManager::GetIdealBounds() const {
  const int shelf_size = ShelfConstants::shelf_size();
  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  gfx::Rect rect(screen_util::GetDisplayBoundsInParent(shelf_window));
  return SelectValueForShelfAlignment(
      gfx::Rect(rect.x(), rect.bottom() - shelf_size, rect.width(), shelf_size),
      gfx::Rect(rect.x(), rect.y(), shelf_size, rect.height()),
      gfx::Rect(rect.right() - shelf_size, rect.y(), shelf_size,
                rect.height()));
}

void ShelfLayoutManager::UpdateVisibilityState() {
  // Bail out early after shelf is destroyed or update is suspended.
  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  if (in_shutdown_ || !shelf_window || suspend_visibility_update_)
    return;

  const wm::WorkspaceWindowState window_state =
      GetShelfWorkspaceWindowState(shelf_window);

  if (shelf_->ShouldHideOnSecondaryDisplay(state_.session_state)) {
    // Needed to hide system tray on secondary display.
    SetState(SHELF_HIDDEN);
  } else if (!state_.IsActiveSessionState()) {
    // Needed to show system tray in non active session state.
    SetState(SHELF_VISIBLE);
  } else if (Shell::Get()->screen_pinning_controller()->IsPinned()) {
    SetState(SHELF_HIDDEN);
  } else {
    // TODO(zelidrag): Verify shelf drag animation still shows on the device
    // when we are in SHELF_AUTO_HIDE_ALWAYS_HIDDEN.
    switch (window_state) {
      case wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN:
        if (IsShelfAutoHideForFullscreenMaximized()) {
          SetState(SHELF_AUTO_HIDE);
        } else if (IsShelfHiddenForFullscreen()) {
          SetState(SHELF_HIDDEN);
        } else {
          // The shelf is sometimes not hidden when in immersive fullscreen.
          // Force the shelf to be auto hidden in this case.
          SetState(SHELF_AUTO_HIDE);
        }
        break;
      case wm::WORKSPACE_WINDOW_STATE_MAXIMIZED:
        SetState(IsShelfAutoHideForFullscreenMaximized()
                     ? SHELF_AUTO_HIDE
                     : CalculateShelfVisibility());
        break;
      case wm::WORKSPACE_WINDOW_STATE_DEFAULT:
        SetState(CalculateShelfVisibility());
        break;
    }
  }

  UpdateWorkspaceMask(window_state);
  SendA11yAlertForFullscreenWorkspaceState(window_state);
}

void ShelfLayoutManager::UpdateAutoHideState() {
  ShelfAutoHideState auto_hide_state =
      CalculateAutoHideState(state_.visibility_state);
  if (auto_hide_state != state_.auto_hide_state) {
    if (auto_hide_state == SHELF_AUTO_HIDE_HIDDEN) {
      // Hides happen immediately.
      SetState(state_.visibility_state);
    } else {
      if (!auto_hide_timer_.IsRunning()) {
        mouse_over_shelf_when_auto_hide_timer_started_ =
            shelf_widget_->GetWindowBoundsInScreen().Contains(
                display::Screen::GetScreen()->GetCursorScreenPoint());
      }
      StartAutoHideTimer();
    }
  } else {
    StopAutoHideTimer();
  }
}

void ShelfLayoutManager::UpdateAutoHideForMouseEvent(ui::MouseEvent* event,
                                                     aura::Window* target) {
  // This also checks IsShelfWindow() and IsStatusAreaWindow() to make sure we
  // don't attempt to hide the shelf if the mouse down occurs on the shelf.
  in_mouse_drag_ = (event->type() == ui::ET_MOUSE_DRAGGED ||
                    (in_mouse_drag_ && event->type() != ui::ET_MOUSE_RELEASED &&
                     event->type() != ui::ET_MOUSE_CAPTURE_CHANGED)) &&
                   !IsShelfWindow(target) && !IsStatusAreaWindow(target);

  // Don't update during shutdown because synthetic mouse events (e.g. mouse
  // exit) may be generated during status area widget teardown.
  if (visibility_state() != SHELF_AUTO_HIDE || in_shutdown_)
    return;

  if (event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSE_MOVED) {
    if (GetVisibleShelfBounds().Contains(
            display::Screen::GetScreen()->GetCursorScreenPoint())) {
      UpdateAutoHideState();
      last_seen_mouse_position_was_over_shelf_ = true;
    } else {
      // The event happened outside the shelf's bounds. If it's a click, hide
      // the shelf immediately. If it's a mouse-out, hide after a delay (but
      // only if it really is a mouse-out, meaning the mouse actually exited the
      // shelf bounds as opposed to having been outside all along).
      if (event->type() == ui::ET_MOUSE_PRESSED)
        UpdateAutoHideState();
      else if (last_seen_mouse_position_was_over_shelf_)
        StartAutoHideTimer();
      last_seen_mouse_position_was_over_shelf_ = false;
    }
  }
}

void ShelfLayoutManager::ProcessGestureEventOfAutoHideShelf(
    ui::GestureEvent* event,
    aura::Window* target) {
  const bool is_shelf_window = IsShelfWindow(target);
  // Skip event processing if shelf widget is fully visible to let the default
  // event dispatching do its work.
  if (IsVisible() || in_shutdown_) {
    // Tap outside of the AUTO_HIDE_SHOWN shelf should hide it.
    if (!is_shelf_window && !IsStatusAreaWindow(target) &&
        visibility_state() == SHELF_AUTO_HIDE &&
        state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN &&
        event->type() == ui::ET_GESTURE_TAP) {
      UpdateAutoHideState();
    }

    // Complete gesture drag when Shelf is visible in auto-hide mode. It is
    // called when swiping Shelf up to show.
    if (is_shelf_window && !IsStatusAreaWindow(target) &&
        visibility_state() == SHELF_AUTO_HIDE &&
        state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN &&
        event->type() == ui::ET_GESTURE_END && drag_status_ != kDragNone) {
      CompleteDrag(*event);
    }

    return;
  }

  if (is_shelf_window) {
    ui::GestureEvent event_in_screen(*event);
    gfx::Point location_in_screen(event->location());
    ::wm::ConvertPointToScreen(target, &location_in_screen);
    event_in_screen.set_location(location_in_screen);
    if (ProcessGestureEvent(event_in_screen))
      event->StopPropagation();
  }
}

void ShelfLayoutManager::AddObserver(ShelfLayoutManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ShelfLayoutManager::RemoveObserver(ShelfLayoutManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool ShelfLayoutManager::ProcessGestureEvent(
    const ui::GestureEvent& event_in_screen) {
  if (!IsDragAllowed())
    return false;

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_BEGIN)
    return StartGestureDrag(event_in_screen);

  if (drag_status_ != kDragInProgress &&
      drag_status_ != kDragAppListInProgress) {
    return false;
  }

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateGestureDrag(event_in_screen);
    return true;
  }

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_END ||
      event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
    if (drag_status_ == kDragAppListInProgress)
      CompleteAppListDrag(event_in_screen);
    else
      CompleteDrag(event_in_screen);
    return true;
  }

  // Unexpected event. Reset the state and let the event fall through.
  CancelDrag();
  return false;
}

void ShelfLayoutManager::ProcessMouseEventFromShelf(
    const ui::MouseEvent& event_in_screen) {
  ui::EventType event_type = event_in_screen.type();
  DCHECK(event_type == ui::ET_MOUSE_PRESSED ||
         event_type == ui::ET_MOUSE_DRAGGED ||
         event_type == ui::ET_MOUSE_RELEASED);

  if (!IsDragAllowed())
    return;

  switch (event_type) {
    case ui::ET_MOUSE_PRESSED:
      AttemptToDragByMouse(event_in_screen);
      break;
    case ui::ET_MOUSE_DRAGGED:
      UpdateMouseDrag(event_in_screen);
      return;
    case ui::ET_MOUSE_RELEASED:
      ReleaseMouseDrag(event_in_screen);
      return;
    default:
      NOTREACHED();
      return;
  }
}

ShelfBackgroundType ShelfLayoutManager::GetShelfBackgroundType() const {
  if (state_.pre_lock_screen_animation_active)
    return SHELF_BACKGROUND_DEFAULT;

  // Handle all other non active screen states, including OOBE and pre-login.
  if (state_.session_state == session_manager::SessionState::OOBE)
    return SHELF_BACKGROUND_OOBE;
  if (state_.session_state != session_manager::SessionState::ACTIVE) {
    if (Shell::Get()->wallpaper_controller()->HasShownAnyWallpaper() &&
        !Shell::Get()->wallpaper_controller()->IsWallpaperBlurred()) {
      return SHELF_BACKGROUND_LOGIN_NONBLURRED_WALLPAPER;
    }
    return SHELF_BACKGROUND_LOGIN;
  }

  if (is_app_list_visible_) {
    if (!IsHomeScreenAvailable())
      return SHELF_BACKGROUND_APP_LIST;

    // When the home screen is available, it is always visible. If the home
    // screen is either fullscreen or being animated or dragged, show the
    // transparent background.
    if (is_home_launcher_shown_ || is_home_launcher_target_position_shown_)
      return SHELF_BACKGROUND_DEFAULT;
  }

  const bool in_split_view_mode =
      Shell::Get()->split_view_controller() &&
      Shell::Get()->split_view_controller()->InSplitViewMode();
  if (in_split_view_mode ||
      (state_.visibility_state != SHELF_AUTO_HIDE &&
       state_.window_state == wm::WORKSPACE_WINDOW_STATE_MAXIMIZED)) {
    return SHELF_BACKGROUND_MAXIMIZED;
  }

  if (Shell::Get()->overview_controller() &&
      Shell::Get()->overview_controller()->InOverviewSession()) {
    return SHELF_BACKGROUND_OVERVIEW;
  }

  return SHELF_BACKGROUND_DEFAULT;
}

void ShelfLayoutManager::MaybeUpdateShelfBackground(AnimationChangeType type) {
  const ShelfBackgroundType new_background_type(GetShelfBackgroundType());

  if (new_background_type == shelf_background_type_)
    return;

  shelf_background_type_ = new_background_type;
  for (auto& observer : observers_)
    observer.OnBackgroundUpdated(shelf_background_type_, type);
}

bool ShelfLayoutManager::ShouldBlurShelfBackground() {
  return is_background_blur_enabled_ &&
         shelf_background_type_ == SHELF_BACKGROUND_DEFAULT &&
         state_.session_state == session_manager::SessionState::ACTIVE;
}

bool ShelfLayoutManager::HasVisibleWindow() const {
  aura::Window* root = shelf_widget_->GetNativeWindow()->GetRootWindow();
  const aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          kActiveDesk);
  // Process the window list and check if there are any visible windows.
  // Ignore app list windows that may be animating to hide after dismissal.
  for (auto* window : windows) {
    if (window->IsVisible() && !IsAppListWindow(window) &&
        root->Contains(window)) {
      return true;
    }
  }
  auto* pip_container = Shell::GetContainer(root, kShellWindowId_PipContainer);
  // The PIP window is not activatable and is not in the MRU list, but count
  // it as a visible window for shelf auto-hide purposes. See crbug.com/942991.
  return !pip_container->children().empty();
}

void ShelfLayoutManager::OnWindowResized() {
  LayoutShelf();
}

void ShelfLayoutManager::SetChildBounds(aura::Window* child,
                                        const gfx::Rect& requested_bounds) {
  wm::WmSnapToPixelLayoutManager::SetChildBounds(child, requested_bounds);
  // We may contain other widgets (such as frame maximize bubble) but they don't
  // effect the layout in anyway.
  if (!updating_bounds_ &&
      ((shelf_widget_->GetNativeWindow() == child) ||
       (shelf_widget_->status_area_widget()->GetNativeWindow() == child))) {
    LayoutShelf();
  }
}

void ShelfLayoutManager::OnShelfAutoHideBehaviorChanged(
    aura::Window* root_window) {
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnUserWorkAreaInsetsChanged(
    aura::Window* root_window) {
  LayoutShelf();
}

void ShelfLayoutManager::OnPinnedStateChanged(aura::Window* pinned_window) {
  // Shelf needs to be hidden on entering to pinned mode, or restored
  // on exiting from pinned mode.
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnSplitViewModeStarted() {
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnSplitViewModeEnded() {
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnOverviewModeStarting() {
  suspend_visibility_update_ = true;
}

void ShelfLayoutManager::OnOverviewModeStartingAnimationComplete(
    bool canceled) {
  suspend_visibility_update_ = false;
  UpdateVisibilityState();
  LayoutShelf();
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnOverviewModeEnding(
    OverviewSession* overview_session) {
  suspend_visibility_update_ = true;
}

void ShelfLayoutManager::OnOverviewModeEndingAnimationComplete(bool canceled) {
  suspend_visibility_update_ = false;
  UpdateVisibilityState();
  LayoutShelf();
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnAppListVisibilityChanged(bool shown,
                                                    int64_t display_id) {
  // Shell may be under destruction.
  if (!shelf_widget_ || !shelf_widget_->GetNativeWindow())
    return;

  if (display_.id() != display_id)
    return;

  is_app_list_visible_ = shown;
  MaybeUpdateShelfBackground(AnimationChangeType::IMMEDIATE);
}

void ShelfLayoutManager::OnHomeLauncherTargetPositionChanged(
    bool showing,
    int64_t display_id) {
  // Shell may be under destruction.
  if (!shelf_widget_ || !shelf_widget_->GetNativeWindow())
    return;

  if (display_.id() != display_id)
    return;

  is_home_launcher_target_position_shown_ = showing;
  MaybeUpdateShelfBackground(AnimationChangeType::IMMEDIATE);
}

void ShelfLayoutManager::OnHomeLauncherAnimationComplete(bool shown,
                                                         int64_t display_id) {
  // Shell may be under destruction.
  if (!shelf_widget_ || !shelf_widget_->GetNativeWindow())
    return;

  if (display_.id() != display_id)
    return;

  is_home_launcher_shown_ = shown;
  is_home_launcher_target_position_shown_ = false;
  MaybeUpdateShelfBackground(AnimationChangeType::IMMEDIATE);

  // Cancel ongoing drag when self is hidden on home screen to prevent
  // visibility issues.
  if (shown && drag_status_ != kDragNone &&
      !Shell::Get()
           ->home_screen_controller()
           ->delegate()
           ->ShouldShowShelfOnHomeScreen()) {
    CancelDrag();
  }
}

void ShelfLayoutManager::OnWindowActivated(ActivationReason reason,
                                           aura::Window* gained_active,
                                           aura::Window* lost_active) {
  UpdateAutoHideStateNow();
}

void ShelfLayoutManager::OnLockStateEvent(LockStateObserver::EventType event) {
  if (event == EVENT_LOCK_ANIMATION_STARTED) {
    // Enter the screen locked state and update the visibility to avoid an odd
    // animation when transitioning the orientation from L/R to bottom.
    state_.pre_lock_screen_animation_active = true;
    UpdateShelfVisibilityAfterLoginUIChange();
  } else {
    state_.pre_lock_screen_animation_active = false;
  }
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Check transition changes to/from the add user to session and change the
  // shelf alignment accordingly
  const bool was_adding_user = state_.IsAddingSecondaryUser();
  const bool was_locked = state_.IsScreenLocked();
  state_.session_state = state;
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
  if (was_adding_user != state_.IsAddingSecondaryUser()) {
    UpdateShelfVisibilityAfterLoginUIChange();
    return;
  }

  // Force the shelf to layout for alignment (bottom if locked, restore the
  // previous alignment otherwise).
  if (was_locked != state_.IsScreenLocked())
    UpdateShelfVisibilityAfterLoginUIChange();

  TargetBounds target_bounds;
  CalculateTargetBoundsAndUpdateWorkArea(&target_bounds);
  UpdateBoundsAndOpacity(target_bounds, true /* animate */, nullptr);
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnLoginStatusChanged(LoginStatus loing_status) {
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnWallpaperBlurChanged() {
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnFirstWallpaperShown() {
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // Update |user_work_area_bounds_| for the new display arrangement.
  TargetBounds target_bounds;
  CalculateTargetBoundsAndUpdateWorkArea(&target_bounds);
}

void ShelfLayoutManager::OnLocaleChanged() {
  // Layout update is needed when language changes between LTR and RTL.
  LayoutShelfAndUpdateBounds();
}

void ShelfLayoutManager::OnScreenCopiedBeforeRotation() {
  if (suspend_visibility_update_) {
    suspend_visibility_update_ = false;
    UpdateVisibilityState();
    LayoutShelf();
    MaybeUpdateShelfBackground(AnimationChangeType::IMMEDIATE);
  }
}

void ShelfLayoutManager::OnScreenRotationAnimationFinished(
    ScreenRotationAnimator* animator,
    bool canceled) {}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, private:

ShelfLayoutManager::TargetBounds::TargetBounds()
    : shelf_opacity(0.0f), status_opacity(0.0f) {}

ShelfLayoutManager::TargetBounds::~TargetBounds() = default;

void ShelfLayoutManager::SetState(ShelfVisibilityState visibility_state) {
  if (suspend_visibility_update_)
    return;

  State state;
  state.visibility_state = visibility_state;
  state.auto_hide_state = CalculateAutoHideState(visibility_state);
  state.is_status_area_visible = CalculateStatusAreaVisibility(state);
  state.window_state =
      GetShelfWorkspaceWindowState(shelf_widget_->GetNativeWindow());

  // Preserve the log in screen states.
  state.session_state = state_.session_state;
  state.pre_lock_screen_animation_active =
      state_.pre_lock_screen_animation_active;

  // Force an update because drag events affect the shelf bounds and we
  // should animate back to the normal bounds at the end of the drag event.
  bool force_update = (drag_status_ == kDragCancelInProgress ||
                       drag_status_ == kDragCompleteInProgress);

  if (!force_update && state_.Equals(state))
    return;  // Nothing changed.

  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(visibility_state);

  StopAutoHideTimer();

  State old_state = state_;
  state_ = state;

  AnimationChangeType change_type = AnimationChangeType::ANIMATE;
  bool delay_background_change = false;

  // Do not animate the background when:
  // - Going from a hidden / auto hidden shelf in fullscreen to a visible shelf
  //   in tablet mode.
  // - Going from an auto hidden shelf in tablet mode to a visible shelf in
  //   tablet mode.
  // - Doing so would result in animating the opacity of the shelf while it is
  //   showing blur.
  if (state.window_state == wm::WORKSPACE_WINDOW_STATE_MAXIMIZED &&
      ((state.visibility_state == SHELF_VISIBLE &&
        old_state.visibility_state != SHELF_VISIBLE) ||
       is_background_blur_enabled_)) {
    change_type = AnimationChangeType::IMMEDIATE;
  } else {
    // Delay the animation when the shelf was hidden, and has just been made
    // visible (e.g. using a gesture-drag).
    if (state.visibility_state == SHELF_VISIBLE &&
        old_state.IsShelfAutoHidden()) {
      delay_background_change = true;
    }
  }

  if (delay_background_change) {
    if (update_shelf_observer_)
      update_shelf_observer_->Detach();
    // |update_shelf_observer_| deletes itself when the animation is done.
    update_shelf_observer_ = new UpdateShelfObserver(this);
  } else {
    MaybeUpdateShelfBackground(change_type);
  }

  TargetBounds target_bounds;
  CalculateTargetBoundsAndUpdateWorkArea(&target_bounds);
  UpdateBoundsAndOpacity(
      target_bounds, true /* animate */,
      delay_background_change ? update_shelf_observer_ : nullptr);

  // Status area widget background changes when it is shown without the shelf
  // and needs to be repainted (but we do not want to do it when not necessary).
  if (ShouldRepaintStatusAreaOnStateChange(old_state, state_))
    shelf_widget_->status_area_widget()->SchedulePaint();

  // OnAutoHideStateChanged Should be emitted when:
  //  - firstly state changed to auto-hide from other state
  //  - or, auto_hide_state has changed
  if ((old_state.visibility_state != state_.visibility_state &&
       state_.visibility_state == SHELF_AUTO_HIDE) ||
      old_state.auto_hide_state != state_.auto_hide_state) {
    for (auto& observer : observers_)
      observer.OnAutoHideStateChanged(state_.auto_hide_state);
  }
}

ShelfVisibilityState ShelfLayoutManager::CalculateShelfVisibility() {
  // Shelf visibility on home screen is controlled by home screen delegate.
  if (IsHomeScreenShown() && !Shell::Get()
                                  ->home_screen_controller()
                                  ->delegate()
                                  ->ShouldShowShelfOnHomeScreen()) {
    return SHELF_HIDDEN;
  }

  switch (shelf_->auto_hide_behavior()) {
    case SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      return SHELF_AUTO_HIDE;
    case SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      return SHELF_VISIBLE;
    case SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
      return SHELF_HIDDEN;
  }
  return SHELF_VISIBLE;
}

void ShelfLayoutManager::LayoutShelfAndUpdateBounds() {
  TargetBounds target_bounds;
  CalculateTargetBoundsAndUpdateWorkArea(&target_bounds);
  UpdateBoundsAndOpacity(target_bounds, false, nullptr);

  // Update insets in ShelfWindowTargeter when shelf bounds change.
  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(visibility_state());
}

void ShelfLayoutManager::LayoutShelf() {
  // The ShelfWidget may be partially closed (no native widget) during shutdown
  // so skip layout.
  if (in_shutdown_)
    return;

  LayoutShelfAndUpdateBounds();
}

void ShelfLayoutManager::UpdateBoundsAndOpacity(
    const TargetBounds& target_bounds,
    bool animate,
    ui::ImplicitAnimationObserver* observer) {
  // Do not update the work area during overview animation.
  if (suspend_visibility_update_)
    return;

  hide_animation_observer_.reset();
  if (GetLayer(shelf_widget_)->opacity() != target_bounds.shelf_opacity) {
    if (target_bounds.shelf_opacity == 0) {
      // On hide, set the opacity after the animation completes.
      hide_animation_observer_ =
          std::make_unique<HideAnimationObserver>(GetLayer(shelf_widget_));
    } else {
      // On show, set the opacity before the animation begins to ensure the blur
      // is shown while the shelf moves.
      GetLayer(shelf_widget_)->SetOpacity(target_bounds.shelf_opacity);
    }
  }

  StatusAreaWidget* status_widget = shelf_widget_->status_area_widget();
  base::AutoReset<bool> auto_reset_updating_bounds(&updating_bounds_, true);
  {
    ui::ScopedLayerAnimationSettings shelf_animation_setter(
        GetLayer(shelf_widget_)->GetAnimator());
    ui::ScopedLayerAnimationSettings status_animation_setter(
        GetLayer(status_widget)->GetAnimator());

    if (hide_animation_observer_)
      shelf_animation_setter.AddObserver(hide_animation_observer_.get());

    if (animate) {
      auto duration = base::TimeDelta::FromMilliseconds(kAnimationDurationMS);
      shelf_animation_setter.SetTransitionDuration(duration);
      shelf_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
      shelf_animation_setter.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      status_animation_setter.SetTransitionDuration(duration);
      status_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
      status_animation_setter.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    } else {
      StopAnimating();
      shelf_animation_setter.SetTransitionDuration(base::TimeDelta());
      status_animation_setter.SetTransitionDuration(base::TimeDelta());
    }
    if (observer)
      status_animation_setter.AddObserver(observer);

    gfx::Rect shelf_bounds = target_bounds.shelf_bounds;
    ::wm::ConvertRectToScreen(shelf_widget_->GetNativeWindow()->parent(),
                              &shelf_bounds);
    shelf_widget_->SetBounds(shelf_bounds);

    GetLayer(status_widget)->SetOpacity(target_bounds.status_opacity);

    // Having a window which is visible but does not have an opacity is an
    // illegal state. We therefore hide the shelf here if required.
    if (!target_bounds.status_opacity)
      status_widget->Hide();
    // Setting visibility during an animation causes the visibility property to
    // animate. Override the animation settings to immediately set the
    // visibility property. Opacity will still animate.

    gfx::Rect status_bounds = target_bounds.status_bounds_in_shelf;
    // When status area is shown without shelf it has the same position as if
    // shelf was visible and fully shown. Therefore position is calculated
    // relatively to the ideal bounds and not the real bounds that lie outside
    // of the screen in that case.
    bool is_showing_status_area_without_shelf =
        state_.is_status_area_visible &&
        state_.visibility_state == SHELF_HIDDEN;
    gfx::Vector2d status_area_offset =
        is_showing_status_area_without_shelf
            ? GetIdealBounds().OffsetFromOrigin()
            : target_bounds.shelf_bounds.OffsetFromOrigin();
    status_bounds.Offset(status_area_offset);
    ::wm::ConvertRectToScreen(status_widget->GetNativeWindow()->parent(),
                              &status_bounds);
    status_widget->SetBounds(status_bounds);

    // Do not update the work area when the alignment changes to BOTTOM_LOCKED
    // to prevent window movement when the screen is locked: crbug.com/622431
    // The work area is initialized with BOTTOM_LOCKED insets to prevent window
    // movement on async preference initialization in tests: crbug.com/834369
    display_ = display::Screen::GetScreen()->GetDisplayNearestWindow(
        shelf_widget_->GetNativeWindow());
    bool in_overview = Shell::Get()->overview_controller()->InOverviewSession();
    if (!in_overview && !state_.IsScreenLocked() &&
        (shelf_->alignment() != SHELF_ALIGNMENT_BOTTOM_LOCKED ||
         display_.work_area() == display_.bounds())) {
      gfx::Insets insets;
      // If user session is blocked (login to new user session or add user to
      // the existing session - multi-profile) then give 100% of work area only
      // if keyboard is not shown.
      // TODO(agawronska): Could this be called from WorkAreaInsets?
      const WorkAreaInsets* const work_area =
          WorkAreaInsets::ForWindow(shelf_widget_->GetNativeWindow());
      if (!state_.IsAddingSecondaryUser() || work_area->IsKeyboardShown())
        insets = work_area->user_work_area_insets();

      Shell::Get()->SetDisplayWorkAreaInsets(shelf_widget_->GetNativeWindow(),
                                             insets);
    }
  }

  // Set an empty border to avoid the shelf view and status area overlapping.
  // TODO(msw): Avoid setting bounds of views within the shelf widget here.
  gfx::Rect shelf_bounds = gfx::Rect(target_bounds.shelf_bounds.size());
  shelf_widget_->GetContentsView()->SetBorder(views::CreateEmptyBorder(
      shelf_bounds.InsetsFrom(target_bounds.shelf_bounds_in_shelf)));
  shelf_widget_->GetContentsView()->Layout();

  // Setting visibility during an animation causes the visibility property to
  // animate. Set the visibility property without an animation.
  if (target_bounds.status_opacity)
    status_widget->Show();
}

bool ShelfLayoutManager::IsDraggingWindowFromTopOrCaptionArea() const {
  // Currently dragging maximized or fullscreen window from the top or the
  // caption area is only allowed in tablet mode.
  if (!IsTabletModeEnabled())
    return false;

  // TODO(minch): Check active window directly if removed search field
  // in overview mode. http://crbug.com/866679
  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (auto* window : windows) {
    wm::WindowState* window_state = wm::GetWindowState(window);
    if (window_state && window_state->is_dragged() &&
        (window_state->IsMaximized() || window_state->IsFullscreen()) &&
        (window_state->drag_details()->window_component == HTCLIENT ||
         window_state->drag_details()->window_component == HTCAPTION)) {
      return true;
    }
  }
  return false;
}

void ShelfLayoutManager::StopAnimating() {
  GetLayer(shelf_widget_)->GetAnimator()->StopAnimating();
  GetLayer(shelf_widget_->status_area_widget())->GetAnimator()->StopAnimating();
}

bool ShelfLayoutManager::IsShowingStatusAreaWithoutShelf() const {
  return state_.is_status_area_visible &&
         state_.visibility_state == SHELF_HIDDEN;
}

void ShelfLayoutManager::CalculateTargetBounds(
    const State& state,
    TargetBounds* target_bounds) const {
  const int shelf_size = ShelfConstants::shelf_size();
  // By default, show the whole shelf on the screen.
  int shelf_in_screen_portion = shelf_size;
  const WorkAreaInsets* const work_area =
      WorkAreaInsets::ForWindow(shelf_widget_->GetNativeWindow());

  if (state.IsShelfAutoHidden()) {
    shelf_in_screen_portion = kHiddenShelfInScreenPortion;
  } else if (state.visibility_state == SHELF_HIDDEN ||
             work_area->IsKeyboardShown()) {
    shelf_in_screen_portion = 0;
  }

  gfx::Rect available_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf_widget_->GetNativeWindow());
  available_bounds.Inset(work_area->GetAccessibilityInsets());

  int shelf_width = PrimaryAxisValue(available_bounds.width(), shelf_size);
  int shelf_height = PrimaryAxisValue(shelf_size, available_bounds.height());
  const int shelf_primary_position = SelectValueForShelfAlignment(
      available_bounds.bottom() - shelf_in_screen_portion,
      available_bounds.x() - shelf_size + shelf_in_screen_portion,
      available_bounds.right() - shelf_in_screen_portion);
  gfx::Point shelf_origin = SelectValueForShelfAlignment(
      gfx::Point(available_bounds.x(), shelf_primary_position),
      gfx::Point(shelf_primary_position, available_bounds.y()),
      gfx::Point(shelf_primary_position, available_bounds.y()));

  target_bounds->shelf_bounds = screen_util::SnapBoundsToDisplayEdge(
      gfx::Rect(shelf_origin.x(), shelf_origin.y(), shelf_width, shelf_height),
      shelf_widget_->GetNativeWindow());

  gfx::Size status_size(
      shelf_widget_->status_area_widget()->GetWindowBoundsInScreen().size());
  if (shelf_->IsHorizontalAlignment())
    status_size.set_height(shelf_size);
  else
    status_size.set_width(shelf_size);

  gfx::Point status_origin = SelectValueForShelfAlignment(
      gfx::Point(0, 0),
      gfx::Point(shelf_width - status_size.width(),
                 shelf_height - status_size.height()),
      gfx::Point(0, shelf_height - status_size.height()));
  if (shelf_->IsHorizontalAlignment() && !base::i18n::IsRTL())
    status_origin.set_x(shelf_width - status_size.width());
  target_bounds->status_bounds_in_shelf = gfx::Rect(status_origin, status_size);

  bool is_showing_status_area_without_shelf =
      state_.is_status_area_visible && state_.visibility_state == SHELF_HIDDEN;
  target_bounds->shelf_opacity = ComputeTargetOpacity(state);
  if (is_showing_status_area_without_shelf) {
    target_bounds->status_opacity = 1.0f;
  } else if (state.IsShelfAutoHidden() && drag_status_ != kDragInProgress) {
    target_bounds->status_opacity = 0.0f;
  } else {
    target_bounds->status_opacity = target_bounds->shelf_opacity;
  }

  if (drag_status_ == kDragInProgress)
    UpdateShelfTargetBoundsForGesture(target_bounds);

  target_bounds->shelf_insets = SelectValueForShelfAlignment(
      gfx::Insets(0, 0, GetShelfInset(state.visibility_state, shelf_height), 0),
      gfx::Insets(0, GetShelfInset(state.visibility_state, shelf_width), 0, 0),
      gfx::Insets(0, 0, 0, GetShelfInset(state.visibility_state, shelf_width)));

  // This needs to happen after calling UpdateTargetBoundsForGesture(), because
  // that can change the size of the shelf.
  target_bounds->shelf_bounds_in_shelf = SelectValueForShelfAlignment(
      gfx::Rect(0, 0, shelf_width - status_size.width(),
                target_bounds->shelf_bounds.height()),
      gfx::Rect(0, 0, target_bounds->shelf_bounds.width(),
                shelf_height - status_size.height()),
      gfx::Rect(0, 0, target_bounds->shelf_bounds.width(),
                shelf_height - status_size.height()));
}

void ShelfLayoutManager::CalculateTargetBoundsAndUpdateWorkArea(
    TargetBounds* target_bounds) {
  CalculateTargetBounds(state_, target_bounds);
  WorkAreaInsets::ForWindow(shelf_widget_->GetNativeWindow())
      ->SetShelfBoundsAndInsets(target_bounds->shelf_bounds,
                                target_bounds->shelf_insets);
}

void ShelfLayoutManager::UpdateShelfTargetBoundsForGesture(
    TargetBounds* target_bounds) const {
  CHECK_EQ(kDragInProgress, drag_status_);
  const bool horizontal = shelf_->IsHorizontalAlignment();
  const int shelf_size = ShelfConstants::shelf_size();
  gfx::Rect available_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf_widget_->GetNativeWindow());
  int resistance_free_region = 0;
  bool hidden_at_start = false;

  if (drag_auto_hide_state_ == SHELF_AUTO_HIDE_HIDDEN &&
      visibility_state() == SHELF_AUTO_HIDE &&
      auto_hide_state() != SHELF_AUTO_HIDE_SHOWN) {
    // If the shelf was hidden when the drag started (and the state hasn't
    // changed since then, e.g. because the tray-menu was shown because of the
    // drag), then allow the drag some resistance-free region at first to make
    // sure the shelf sticks with the finger until the shelf is visible.
    resistance_free_region = shelf_size - kHiddenShelfInScreenPortion;
    hidden_at_start = true;
  }

  bool resist = SelectValueForShelfAlignment(
      drag_amount_ < -resistance_free_region, drag_amount_ >
          resistance_free_region,
      drag_amount_ < -resistance_free_region);

  float translate = 0.f;
  if (resist) {
    float diff = fabsf(drag_amount_) - resistance_free_region;
    diff = std::min(diff, sqrtf(diff));
    if (drag_amount_ < 0)
      translate = -resistance_free_region - diff;
    else
      translate = resistance_free_region + diff;
  } else {
    translate = drag_amount_;
  }
  // Move the shelf with the gesture.
  const int baseline = SelectValueForShelfAlignment(
      available_bounds.bottom() - (hidden_at_start ? 0 : shelf_size),
      available_bounds.x() - (hidden_at_start ? shelf_size : 0),
      available_bounds.right() - (hidden_at_start ? 0 : shelf_size));
  if (horizontal) {
    target_bounds->shelf_bounds.set_y(baseline + translate);
    target_bounds->status_bounds_in_shelf.set_y(0);
  } else {
    target_bounds->shelf_bounds.set_x(baseline + translate);
    target_bounds->status_bounds_in_shelf.set_x(0);
  }
}

void ShelfLayoutManager::UpdateAutoHideStateNow() {
  SetState(state_.visibility_state);

  // If the state did not change, the auto hide timer may still be running.
  StopAutoHideTimer();
}

void ShelfLayoutManager::StartAutoHideTimer() {
  auto_hide_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(kAutoHideDelayMS),
                         this, &ShelfLayoutManager::UpdateAutoHideStateNow);
}

void ShelfLayoutManager::StopAutoHideTimer() {
  auto_hide_timer_.Stop();
  mouse_over_shelf_when_auto_hide_timer_started_ = false;
}

gfx::Rect ShelfLayoutManager::GetVisibleShelfBounds() const {
  gfx::Rect shelf_region = shelf_widget_->GetWindowBoundsInScreen();
  DCHECK(!display_.bounds().IsEmpty());
  shelf_region.Intersect(display_.bounds());
  return screen_util::SnapBoundsToDisplayEdge(shelf_region,
                                              shelf_widget_->GetNativeWindow());
}

gfx::Rect ShelfLayoutManager::GetAutoHideShowShelfRegionInScreen() const {
  gfx::Rect shelf_bounds_in_screen = GetVisibleShelfBounds();
  gfx::Vector2d offset = SelectValueForShelfAlignment(
      gfx::Vector2d(0, shelf_bounds_in_screen.height()),
      gfx::Vector2d(-kMaxAutoHideShowShelfRegionSize, 0),
      gfx::Vector2d(shelf_bounds_in_screen.width(), 0));

  gfx::Rect show_shelf_region_in_screen = shelf_bounds_in_screen;
  show_shelf_region_in_screen += offset;
  if (shelf_->IsHorizontalAlignment())
    show_shelf_region_in_screen.set_height(kMaxAutoHideShowShelfRegionSize);
  else
    show_shelf_region_in_screen.set_width(kMaxAutoHideShowShelfRegionSize);

  // TODO(pkotwicz): Figure out if we need any special handling when the
  // keyboard is visible.
  return screen_util::SnapBoundsToDisplayEdge(show_shelf_region_in_screen,
                                              shelf_widget_->GetNativeWindow());
}

ShelfAutoHideState ShelfLayoutManager::CalculateAutoHideState(
    ShelfVisibilityState visibility_state) const {
  if (is_auto_hide_state_locked_)
    return state_.auto_hide_state;

  // Shelf is not available before login.
  // TODO(crbug.com/701157): Remove this when the login webui fake-shelf is
  // replaced with views.
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return SHELF_AUTO_HIDE_HIDDEN;

  if (visibility_state != SHELF_AUTO_HIDE)
    return SHELF_AUTO_HIDE_HIDDEN;

  // Don't update the auto hide state if it is locked.
  if (shelf_->auto_hide_lock())
    return state_.auto_hide_state;

  if (shelf_widget_->IsShowingAppList() && !IsHomeScreenAvailable())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->ShouldShowShelf()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (shelf_widget_->IsShowingMenu())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->IsShowingOverflowBubble())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->IsActive() ||
      (shelf_widget_->status_area_widget() &&
       shelf_widget_->status_area_widget()->IsActive())) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  // If there are no visible windows do not hide the shelf.
  if (!HasVisibleWindow())
    return SHELF_AUTO_HIDE_SHOWN;

  if (IsDraggingWindowFromTopOrCaptionArea())
    return SHELF_AUTO_HIDE_SHOWN;

  // Do not hide the shelf if overview mode is active.
  if (Shell::Get()->overview_controller() &&
      Shell::Get()->overview_controller()->InOverviewSession()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (drag_status_ == kDragAppListInProgress)
    return SHELF_AUTO_HIDE_SHOWN;

  if (drag_status_ == kDragCompleteInProgress ||
      drag_status_ == kDragCancelInProgress) {
    return drag_auto_hide_state_;
  }

  // Don't show if the user is dragging the mouse.
  if (in_mouse_drag_)
    return SHELF_AUTO_HIDE_HIDDEN;

  gfx::Rect shelf_region = GetVisibleShelfBounds();
  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->IsMessageBubbleShown() &&
      IsVisible()) {
    // Increase the the hit test area to prevent the shelf from disappearing
    // when the mouse is over the bubble gap.
    ShelfAlignment alignment = shelf_->alignment();
    shelf_region.Inset(
        alignment == SHELF_ALIGNMENT_RIGHT ? -kNotificationBubbleGapHeight : 0,
        shelf_->IsHorizontalAlignment() ? -kNotificationBubbleGapHeight : 0,
        alignment == SHELF_ALIGNMENT_LEFT ? -kNotificationBubbleGapHeight : 0,
        0);
  }

  // Do not perform any checks based on the cursor position if the mouse cursor
  // is currently hidden.
  if (!shelf_widget_->IsMouseEventsEnabled())
    return SHELF_AUTO_HIDE_HIDDEN;

  gfx::Point cursor_position_in_screen =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  // Cursor is invisible in tablet mode and plug in an external mouse in tablet
  // mode will switch to clamshell mode.
  if (shelf_region.Contains(cursor_position_in_screen) &&
      !IsTabletModeEnabled()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  // When the shelf is auto hidden and the shelf is on the boundary between two
  // displays, it is hard to trigger showing the shelf. For instance, if a
  // user's primary display is left of their secondary display, it is hard to
  // unautohide a left aligned shelf on the secondary display.
  // It is hard because:
  // - It is hard to stop the cursor in the shelf "light bar" and not overshoot.
  // - The cursor is warped to the other display if the cursor gets to the edge
  //   of the display.
  // Show the shelf if the cursor started on the shelf and the user overshot the
  // shelf slightly to make it easier to show the shelf in this situation. We
  // do not check |auto_hide_timer_|.IsRunning() because it returns false when
  // the timer's task is running.
  if ((state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN ||
       mouse_over_shelf_when_auto_hide_timer_started_) &&
      GetAutoHideShowShelfRegionInScreen().Contains(
          cursor_position_in_screen)) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  return SHELF_AUTO_HIDE_HIDDEN;
}

bool ShelfLayoutManager::CalculateStatusAreaVisibility(
    const ShelfLayoutManager::State& state) const {
  // Shelf visibility on home screen is controlled by home screen delegate,
  // otherwise it matches shelf visibility.
  if (!IsHomeScreenShown())
    return state.IsShelfVisible();

  HomeScreenDelegate* delegate =
      Shell::Get()->home_screen_controller()->delegate();
  // Shelf visible and status hidden is currently unsupported.
  DCHECK(delegate->ShouldShowStatusAreaOnHomeScreen() ||
         !delegate->ShouldShowShelfOnHomeScreen());
  return delegate->ShouldShowStatusAreaOnHomeScreen();
}

bool ShelfLayoutManager::ShouldRepaintStatusAreaOnStateChange(
    ShelfLayoutManager::State old_state,
    ShelfLayoutManager::State new_state) const {

  if (old_state.visibility_state == new_state.visibility_state)
    return false;

  return (old_state.visibility_state != SHELF_HIDDEN &&
          new_state.visibility_state == SHELF_HIDDEN) ||
         (old_state.visibility_state == SHELF_HIDDEN &&
          new_state.visibility_state != SHELF_HIDDEN &&
          old_state.is_status_area_visible);
}

bool ShelfLayoutManager::IsShelfWindow(aura::Window* window) {
  if (!window)
    return false;
  const aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  return shelf_window && shelf_window->Contains(window);
}

bool ShelfLayoutManager::IsStatusAreaWindow(aura::Window* window) {
  if (!window)
    return false;
  const aura::Window* status_window =
      shelf_widget_->status_area_widget()->GetNativeWindow();
  return status_window && status_window->Contains(window);
}

void ShelfLayoutManager::UpdateShelfVisibilityAfterLoginUIChange() {
  UpdateVisibilityState();
  LayoutShelf();
}

float ShelfLayoutManager::ComputeTargetOpacity(const State& state) const {
  if (drag_status_ == kDragInProgress ||
      state.visibility_state == SHELF_VISIBLE) {
    return 1.0f;
  }
  // In Chrome OS Material Design, when shelf is hidden during auto hide state,
  // target bounds are also hidden. So the window can extend to the edge of
  // screen.
  return (state.visibility_state == SHELF_AUTO_HIDE &&
          state.auto_hide_state == SHELF_AUTO_HIDE_SHOWN)
             ? 1.0f
             : 0.0f;
}

bool ShelfLayoutManager::IsShelfHiddenForFullscreen() const {
  // If the non-fullscreen app list should be shown, the shelf should not be
  // hidden.
  if (!IsHomeScreenAvailable() &&
      Shell::Get()->app_list_controller()->GetTargetVisibility()) {
    return false;
  }

  const aura::Window* fullscreen_window = wm::GetWindowForFullscreenModeInRoot(
      shelf_widget_->GetNativeWindow()->GetRootWindow());
  return fullscreen_window &&
         wm::GetWindowState(fullscreen_window)->GetHideShelfWhenFullscreen();
}

bool ShelfLayoutManager::IsShelfAutoHideForFullscreenMaximized() const {
  wm::WindowState* active_window = wm::GetActiveWindowState();
  return active_window &&
         active_window->autohide_shelf_when_maximized_or_fullscreen();
}

bool ShelfLayoutManager::IsHomeScreenShown() const {
  if (!IsHomeScreenAvailable())
    return false;

  return !Shell::Get()->overview_controller()->InOverviewSession() &&
         !HasVisibleWindow();
}

bool ShelfLayoutManager::ShouldHomeGestureHandleEvent(float scroll_y) const {
  // If the shelf is not visible, home gesture shouldn't trigger.
  if (!IsVisible())
    return false;

  // Scroll down events should never be handled, unless they are currently being
  // handled
  if (scroll_y >= 0 && drag_status_ != kDragAppListInProgress)
    return false;

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Gesture drag related functions:
bool ShelfLayoutManager::StartGestureDrag(
    const ui::GestureEvent& gesture_in_screen) {
  if (drag_status_ != kDragNone)
    return false;

  float scroll_y_hint = gesture_in_screen.details().scroll_y_hint();
  if (StartAppListDrag(gesture_in_screen, scroll_y_hint))
    return true;

  if (ShouldHomeGestureHandleEvent(scroll_y_hint)) {
    DragStatus previous_drag_status = drag_status_;
    drag_status_ = kDragAppListInProgress;
    HomeLauncherGestureHandler* home_launcher_handler =
        Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
    if (home_launcher_handler->OnPressEvent(
            HomeLauncherGestureHandler::Mode::kSlideUpToShow,
            gesture_in_screen.location())) {
      return true;
    } else {
      drag_status_ = previous_drag_status;
    }
  }

  return StartShelfDrag();
}

void ShelfLayoutManager::UpdateGestureDrag(
    const ui::GestureEvent& gesture_in_screen) {
  float scroll_x = gesture_in_screen.details().scroll_x();
  float scroll_y = gesture_in_screen.details().scroll_y();

  if (ShouldHomeGestureHandleEvent(scroll_y)) {
    HomeLauncherGestureHandler* home_launcher_handler =
        Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
    if (home_launcher_handler->OnScrollEvent(gesture_in_screen.location(),
                                             scroll_y)) {
      return;
    }
  }

  UpdateDrag(gesture_in_screen, scroll_x, scroll_y);
}

////////////////////////////////////////////////////////////////////////////////
// Mouse drag related functions:

void ShelfLayoutManager::AttemptToDragByMouse(
    const ui::MouseEvent& mouse_in_screen) {
  if (drag_status_ != kDragNone)
    return;

  drag_status_ = kDragAttempt;
  last_mouse_drag_position_ = mouse_in_screen.location();
}

void ShelfLayoutManager::StartMouseDrag(const ui::MouseEvent& mouse_in_screen) {
  float scroll_y_hint = mouse_in_screen.y() - last_mouse_drag_position_.y();
  if (!StartAppListDrag(mouse_in_screen, scroll_y_hint))
    StartShelfDrag();
}

void ShelfLayoutManager::UpdateMouseDrag(
    const ui::MouseEvent& mouse_in_screen) {
  if (drag_status_ == kDragNone)
    return;

  DCHECK(drag_status_ == kDragAttempt || drag_status_ == kDragInProgress ||
         drag_status_ == kDragAppListInProgress);

  if (drag_status_ == kDragAttempt) {
    // Do not start drag for the small offset.
    if (abs(mouse_in_screen.location().y() - last_mouse_drag_position_.y()) <
      kMouseDragThreshold) {
      return;
    }

    // Mouse events do not provide the drag offset like gesture events. So
    // start mouse drag when mouse is moved.
    StartMouseDrag(mouse_in_screen);
  } else {
    int scroll_x =
        mouse_in_screen.location().x() - last_mouse_drag_position_.x();
    int scroll_y =
        mouse_in_screen.location().y() - last_mouse_drag_position_.y();
    UpdateDrag(mouse_in_screen, scroll_x, scroll_y);
    last_mouse_drag_position_ = mouse_in_screen.location();
  }
}

void ShelfLayoutManager::ReleaseMouseDrag(
    const ui::MouseEvent& mouse_in_screen) {
  if (drag_status_ == kDragNone)
    return;

  DCHECK(drag_status_ == kDragAttempt ||
         drag_status_ == kDragAppListInProgress ||
         drag_status_ == kDragInProgress);

  switch (drag_status_) {
    case kDragAttempt:
      drag_status_ = kDragNone;
      break;
    case kDragAppListInProgress:
      CompleteAppListDrag(mouse_in_screen);
      break;
    case kDragInProgress:
      CompleteDrag(mouse_in_screen);
      break;
    default:
      NOTREACHED();
  }
  last_mouse_drag_position_ = gfx::Point();
}

////////////////////////////////////////////////////////////////////////////////
// Drag related functions:

bool ShelfLayoutManager::IsDragAllowed() const {
  // The gestures are disabled in the lock/login screen.
  SessionControllerImpl* controller = Shell::Get()->session_controller();
  if (!controller->NumberOfLoggedInUsers() || controller->IsScreenLocked())
    return false;

  if (IsShelfHiddenForFullscreen())
    return false;

  // Gestures are disabled when status area is visible without shelf.
  if (state_.is_status_area_visible && state_.visibility_state == SHELF_HIDDEN)
    return false;

  return true;
}

bool ShelfLayoutManager::StartAppListDrag(
    const ui::LocatedEvent& event_in_screen,
    float scroll_y_hint) {
  // If the home screen is available, gesture dragging is handled by
  // HomeLauncherGestureHandler.
  if (IsHomeScreenAvailable() && event_in_screen.IsGestureEvent())
    return false;

  // Fullscreen app list can only be dragged from bottom alignment shelf.
  if (!shelf_->IsHorizontalAlignment())
    return false;

  // If the shelf is not visible, swiping up should show the shelf.
  if (!IsVisible())
    return false;

  // Do not show the fullscreen app list until the overflow bubble has been
  // closed.
  if (shelf_widget_->IsShowingOverflowBubble())
    return false;

  // If app list is already opened, swiping up on the shelf should keep the app
  // list opened.
  if (is_app_list_visible_)
    return false;

  // Swipes down on shelf should hide the shelf.
  if (scroll_y_hint >= 0)
    return false;

  const gfx::Rect shelf_bounds = GetIdealBounds();
  shelf_background_type_before_drag_ = shelf_background_type_;
  drag_status_ = kDragAppListInProgress;

  // TODO(michaelpg): Simplify gesture drag logic and remove these DCHECKs.
  DCHECK(Shell::Get()->app_list_controller());
  Shell::Get()->app_list_controller()->Show(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(shelf_widget_->GetNativeWindow())
          .id(),
      app_list::kSwipeFromShelf, event_in_screen.time_stamp());
  Shell::Get()->app_list_controller()->UpdateYPositionAndOpacity(
      shelf_bounds.y(), GetAppListBackgroundOpacityOnShelfOpacity());
  launcher_above_shelf_bottom_amount_ =
      shelf_bounds.bottom() - event_in_screen.location().y();

  return true;
}

bool ShelfLayoutManager::StartShelfDrag() {
  // Disable the shelf dragging if the fullscreen app list is opened.
  if (is_app_list_visible_ && !IsHomeScreenAvailable())
    return false;

  // Also disable shelf drags until the overflow shelf is closed.
  if (shelf_widget_->IsShowingOverflowBubble())
    return false;

  drag_status_ = kDragInProgress;
  drag_auto_hide_state_ = visibility_state() == SHELF_AUTO_HIDE
                              ? auto_hide_state()
                              : SHELF_AUTO_HIDE_SHOWN;
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
  drag_amount_ = 0.f;
  return true;
}

void ShelfLayoutManager::UpdateDrag(const ui::LocatedEvent& event_in_screen,
                                    float scroll_x,
                                    float scroll_y) {
  if (drag_status_ == kDragAppListInProgress) {
    // Dismiss the app list if the shelf changed to vertical alignment during
    // dragging.
    DCHECK(Shell::Get()->app_list_controller());
    if (!shelf_->IsHorizontalAlignment()) {
      Shell::Get()->app_list_controller()->DismissAppList();
      launcher_above_shelf_bottom_amount_ = 0.f;
      drag_status_ = kDragNone;
      return;
    }
    const gfx::Rect shelf_bounds = GetIdealBounds();
    Shell::Get()->app_list_controller()->UpdateYPositionAndOpacity(
        std::min(event_in_screen.location().y(), shelf_bounds.y()),
        GetAppListBackgroundOpacityOnShelfOpacity());
    launcher_above_shelf_bottom_amount_ =
        shelf_bounds.bottom() - event_in_screen.location().y();
  } else {
    drag_amount_ += PrimaryAxisValue(scroll_y, scroll_x);
    LayoutShelf();
  }
}

void ShelfLayoutManager::CompleteDrag(const ui::LocatedEvent& event_in_screen) {
  if (!ShouldChangeVisibilityAfterDrag(event_in_screen)) {
    CancelDrag();
    return;
  }

  shelf_widget_->Deactivate();
  shelf_widget_->status_area_widget()->Deactivate();

  drag_auto_hide_state_ = drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN
                              ? SHELF_AUTO_HIDE_HIDDEN
                              : SHELF_AUTO_HIDE_SHOWN;

  // Gesture drag will only change the auto hide state of the shelf but not the
  // auto hide behavior. Auto hide behavior can only be changed through the
  // context menu of the shelf. Set |drag_status_| to
  // kDragCompleteInProgress to set the auto hide state to
  // |drag_auto_hide_state_|.
  drag_status_ = kDragCompleteInProgress;
  UpdateVisibilityState();
  drag_status_ = kDragNone;
}

void ShelfLayoutManager::CompleteAppListDrag(
    const ui::LocatedEvent& event_in_screen) {
  // Change the shelf alignment to vertical during drag will reset
  // |drag_status_| to |kDragNone|.
  if (drag_status_ == kDragNone)
    return;

  HomeLauncherGestureHandler* home_launcher_handler =
      Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
  DCHECK(home_launcher_handler);
  if (home_launcher_handler->OnReleaseEvent(event_in_screen.location())) {
    drag_status_ = kDragNone;
    return;
  }
  DCHECK(Shell::Get()->app_list_controller());

  using ash::AppListViewState;
  AppListViewState app_list_state =
      Shell::Get()->app_list_controller()->CalculateStateAfterShelfDrag(
          event_in_screen, launcher_above_shelf_bottom_amount_);

  // Keep auto-hide shelf visible if failed to open the app list.
  base::Optional<Shelf::ScopedAutoHideLock> auto_hide_lock;
  if (app_list_state == AppListViewState::kClosed)
    auto_hide_lock.emplace(shelf_);
  Shell::Get()->app_list_controller()->EndDragFromShelf(app_list_state);
  drag_status_ = kDragNone;
}

void ShelfLayoutManager::CancelDrag() {
  if (drag_status_ == kDragAppListInProgress) {
    HomeLauncherGestureHandler* home_launcher_handler =
        Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
    DCHECK(home_launcher_handler);
    if (home_launcher_handler->IsDragInProgress())
      home_launcher_handler->Cancel();
    else {
      DCHECK(Shell::Get()->app_list_controller());
      Shell::Get()->app_list_controller()->DismissAppList();
    }
  } else {
    // Set |drag_status_| to kDragCancelInProgress to set the
    // auto hide state to |drag_auto_hide_state_|, which is the
    // visibility state before starting drag.
    drag_status_ = kDragCancelInProgress;
    UpdateVisibilityState();
  }
  drag_status_ = kDragNone;
}

float ShelfLayoutManager::GetAppListBackgroundOpacityOnShelfOpacity() {
  float shelf_opacity = shelf_widget_->GetBackgroundAlphaValue(
                            shelf_background_type_before_drag_) /
                        static_cast<float>(ShelfBackgroundAnimator::kMaxAlpha);
  const int shelf_size = ShelfConstants::shelf_size();
  if (launcher_above_shelf_bottom_amount_ < shelf_size)
    return shelf_opacity;
  float launcher_above_shelf_amount =
      std::max(0.f, launcher_above_shelf_bottom_amount_ - shelf_size);
  float coefficient =
      std::min(launcher_above_shelf_amount /
                   (app_list::AppListView::kNumOfShelfSize * shelf_size),
               1.0f);
  float app_list_view_opacity =
      is_background_blur_enabled_
          ? app_list::AppListView::kAppListOpacityWithBlur
          : app_list::AppListView::kAppListOpacity;
  return app_list_view_opacity * coefficient +
         (1 - coefficient) * shelf_opacity;
}

bool ShelfLayoutManager::IsSwipingCorrectDirection() {
  switch (shelf_->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
    case SHELF_ALIGNMENT_RIGHT:
      if (drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN)
        return drag_amount_ > 0;
      return drag_amount_ < 0;
    case SHELF_ALIGNMENT_LEFT:
      if (drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN)
        return drag_amount_ < 0;
      return drag_amount_ > 0;
  }
  return false;
}

bool ShelfLayoutManager::ShouldChangeVisibilityAfterDrag(
    const ui::LocatedEvent& event_in_screen) {
  // Shelf can be visible in 1) SHELF_VISIBLE or 2) SHELF_AUTO_HIDE but in
  // SHELF_AUTO_HIDE_SHOWN. See details in IsVisible. Dragging on SHELF_VISIBLE
  // shelf should not change its visibility since it should be kept visible.
  if (visibility_state() == SHELF_VISIBLE)
    return false;

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_END ||
      event_in_screen.type() == ui::ET_MOUSE_RELEASED) {
    // The visibility of the shelf changes only if the shelf was dragged X%
    // along the correct axis. If the shelf was already visible, then the
    // direction of the drag does not matter.
    const float kDragHideThreshold = 0.4f;
    const gfx::Rect bounds = GetIdealBounds();
    const float drag_ratio =
        fabs(drag_amount_) /
        (shelf_->IsHorizontalAlignment() ? bounds.height() : bounds.width());

    return IsSwipingCorrectDirection() && drag_ratio > kDragHideThreshold;
  }

  if (event_in_screen.type() == ui::ET_SCROLL_FLING_START)
    return IsSwipingCorrectDirection();

  return false;
}

void ShelfLayoutManager::UpdateWorkspaceMask(
    wm::WorkspaceWindowState window_state) {
  // Disable the mask on NonLockScreenContainer if maximized/fullscreen window
  // is on top.
  // TODO(oshima): Investigate if we can remove SetMasksToBounds calls
  // crbug.com/898236.
  auto* root_window_controller =
      RootWindowController::ForWindow(shelf_widget_->GetNativeWindow());
  auto* container = root_window_controller->GetContainer(
      kShellWindowId_NonLockScreenContainersContainer);
  switch (window_state) {
    case wm::WORKSPACE_WINDOW_STATE_MAXIMIZED:
    case wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN:
      container->layer()->SetMasksToBounds(false);
      break;
    case wm::WORKSPACE_WINDOW_STATE_DEFAULT:
      container->layer()->SetMasksToBounds(true);
      break;
  }
}

void ShelfLayoutManager::SendA11yAlertForFullscreenWorkspaceState(
    wm::WorkspaceWindowState current_workspace_window_state) {
  if (previous_workspace_window_state_ !=
          wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN &&
      current_workspace_window_state ==
          wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN) {
    Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
        mojom::AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED);
  } else if (previous_workspace_window_state_ ==
                 wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN &&
             current_workspace_window_state !=
                 wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN) {
    Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
        mojom::AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_EXITED);
  }
  previous_workspace_window_state_ = current_workspace_window_state;
}

}  // namespace ash
