// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_LAYOUT_MANAGER_H_
#define ASH_SHELF_SHELF_LAYOUT_MANAGER_H_

#include <memory>

#include "ash/app_list/app_list_controller_observer.h"
#include "ash/ash_export.h"
#include "ash/home_screen/home_launcher_gesture_handler_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/wallpaper_controller.h"
#include "ash/public/cpp/wallpaper_controller_observer.h"
#include "ash/session/session_observer.h"
#include "ash/shelf/shelf.h"
#include "ash/shell_observer.h"
#include "ash/system/locale/locale_update_controller_impl.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/lock_state_observer.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/wm_default_layout_manager.h"
#include "ash/wm/workspace/workspace_types.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ui {
class EventHandler;
class ImplicitAnimationObserver;
class LocatedEvent;
class MouseEvent;
}  // namespace ui

namespace ash {

enum class AnimationChangeType;
class PanelLayoutManagerTest;
class Shelf;
class ShelfLayoutManagerObserver;
class ShelfLayoutManagerTest;
class ShelfWidget;

// ShelfLayoutManager is the layout manager responsible for the shelf and
// status widgets. The shelf is given the total available width and told the
// width of the status area. This allows the shelf to draw the background and
// layout to the status area.
// To respond to bounds changes in the status area StatusAreaLayoutManager works
// closely with ShelfLayoutManager.
// On mus, widget bounds management is handled by the window manager.
class ASH_EXPORT ShelfLayoutManager : public AppListControllerObserver,
                                      public HomeLauncherGestureHandlerObserver,
                                      public ShellObserver,
                                      public OverviewObserver,
                                      public ::wm::ActivationChangeObserver,
                                      public LockStateObserver,
                                      public WmDefaultLayoutManager,
                                      public display::DisplayObserver,
                                      public SessionObserver,
                                      public WallpaperControllerObserver,
                                      public LocaleChangeObserver,
                                      public DesksController::Observer {
 public:
  // Suspend shelf visibility update within its scope. Note that relevant
  // ShelfLayoutManager must outlive this class.
  class ScopedSuspendVisibilityUpdate {
   public:
    // |manager| is the ShelfLayoutManager whose visibility update is suspended.
    explicit ScopedSuspendVisibilityUpdate(ShelfLayoutManager* manager);
    ~ScopedSuspendVisibilityUpdate();

   private:
    ShelfLayoutManager* const manager_;
    DISALLOW_COPY_AND_ASSIGN(ScopedSuspendVisibilityUpdate);
  };

  ShelfLayoutManager(ShelfWidget* shelf_widget, Shelf* shelf);
  ~ShelfLayoutManager() override;

  // Initializes observers.
  void InitObservers();

  // Clears internal data for shutdown process.
  void PrepareForShutdown();
  // Returns whether the shelf and its contents (shelf, status) are visible
  // on the screen.
  bool IsVisible() const;

  // Returns the ideal bounds of the shelf assuming it is visible.
  gfx::Rect GetIdealBounds() const;

  // Stops any animations, sets the bounds of the shelf and status widgets, and
  // changes the work area
  void LayoutShelf();

  // Updates the visibility state.
  void UpdateVisibilityState();

  // Invoked by the shelf when the auto-hide state may have changed.
  void UpdateAutoHideState();

  // Updates the auto-hide state for mouse events.
  void UpdateAutoHideForMouseEvent(ui::MouseEvent* event, aura::Window* target);

  // Called by AutoHideEventHandler to process the gesture events when shelf is
  // auto hide.
  void ProcessGestureEventOfAutoHideShelf(ui::GestureEvent* event,
                                          aura::Window* target);

  // Handles events that are detected while the hotseat is kExtended in in-app
  // shelf.
  void ProcessGestureEventOfInAppHotseat(ui::GestureEvent* event,
                                         aura::Window* target);

  void AddObserver(ShelfLayoutManagerObserver* observer);
  void RemoveObserver(ShelfLayoutManagerObserver* observer);

  // Processes a gesture event and updates the status of the shelf when
  // appropriate. Returns true if the gesture has been handled and it should not
  // be processed any further, false otherwise.
  bool ProcessGestureEvent(const ui::GestureEvent& event_in_screen);

  // Handles mouse events from the shelf.
  void ProcessMouseEventFromShelf(const ui::MouseEvent& event_in_screen);

  // Handles events from ShelfWidget.
  void ProcessGestureEventFromShelfWidget(ui::GestureEvent* event_in_screen);

  // Returns how the shelf background should be painted.
  ShelfBackgroundType GetShelfBackgroundType() const;

  // Updates the background of the shelf if it has changed.
  void MaybeUpdateShelfBackground(AnimationChangeType change_type);

  // Handles events from the HotseatWidget.
  void ProcessGestureEventFromHotseatWidget(ui::GestureEvent* event,
                                            aura::Window* target);

  // Returns whether the shelf should show a blurred background. This may
  // return false even if background blur is enabled depending on the session
  // state.
  bool ShouldBlurShelfBackground();

  // Returns true if at least one window is visible.
  bool HasVisibleWindow() const;

  // Cancel the drag if the shelf is in drag progress.
  void CancelDragOnShelfIfInProgress();

  // Suspends/resumes visibility update. Use ScopedSuspendVisibilityUpdate when
  // possible to ensure there are balanced calls.
  void SuspendVisibilityUpdate();
  void ResumeVisiblityUpdate();

  // WmDefaultLayoutManager:
  void OnWindowResized() override;
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override;

  // ShellObserver:
  void OnShelfAutoHideBehaviorChanged(aura::Window* root_window) override;
  void OnUserWorkAreaInsetsChanged(aura::Window* root_window) override;
  void OnPinnedStateChanged(aura::Window* pinned_window) override;
  void OnSplitViewModeStarted() override;
  void OnSplitViewModeEnded() override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeStartingAnimationComplete(bool canceled) override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // AppListControllerObserver:
  void OnAppListVisibilityChanged(bool shown, int64_t display_id) override;

  // HomeLauncherGestureHandlerObserver:
  void OnHomeLauncherTargetPositionChanged(bool showing,
                                           int64_t display_id) override;
  void OnHomeLauncherAnimationComplete(bool shown, int64_t display_id) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // LockStateObserver:
  void OnLockStateEvent(LockStateObserver::EventType event) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;
  void OnLoginStatusChanged(LoginStatus loing_status) override;

  // WallpaperControllerObserver:
  void OnWallpaperBlurChanged() override;
  void OnFirstWallpaperShown() override;

  // DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // LocaleChangeObserver:
  void OnLocaleChanged() override;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override {}
  void OnDeskRemoved(const Desk* desk) override {}
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {}
  void OnDeskSwitchAnimationLaunching() override;
  void OnDeskSwitchAnimationFinished() override;

  ShelfVisibilityState visibility_state() const {
    return state_.visibility_state;
  }

  void LockAutoHideState(bool lock_auto_hide_state) {
    is_auto_hide_state_locked_ = lock_auto_hide_state;
  }

  bool updating_bounds() const { return updating_bounds_; }
  ShelfAutoHideState auto_hide_state() const { return state_.auto_hide_state; }

  // TODO(harrym|oshima): These templates will be moved to a new Shelf class.
  // A helper function for choosing values specific to a shelf alignment.
  template <typename T>
  T SelectValueForShelfAlignment(T bottom, T left, T right) const {
    switch (shelf_->alignment()) {
      case SHELF_ALIGNMENT_BOTTOM:
      case SHELF_ALIGNMENT_BOTTOM_LOCKED:
        return bottom;
      case SHELF_ALIGNMENT_LEFT:
        return left;
      case SHELF_ALIGNMENT_RIGHT:
        return right;
    }
    NOTREACHED();
    return right;
  }

  template <typename T>
  T PrimaryAxisValue(T horizontal, T vertical) const {
    return shelf_->IsHorizontalAlignment() ? horizontal : vertical;
  }

 private:
  class UpdateShelfObserver;
  friend class PanelLayoutManagerTest;
  friend class ShelfLayoutManagerTest;
  friend class NotificationTrayTest;

  struct TargetBounds {
    TargetBounds();
    ~TargetBounds();

    float opacity;

    gfx::Rect shelf_bounds;             // Bounds of the shelf within the screen
    gfx::Rect shelf_bounds_in_shelf;    // Bounds of the shelf minus status area
    gfx::Rect nav_bounds_in_shelf;      // Bounds of nav widget within shelf
    gfx::Rect hotseat_bounds_in_shelf;  // Bounds of the hotseat within shelf
    gfx::Rect status_bounds_in_shelf;   // Bounds of status area within shelf
    gfx::Insets shelf_insets;           // Shelf insets within the screen
  };

  struct State {
    State();

    // Returns true when a secondary user is being added to an existing session.
    bool IsAddingSecondaryUser() const;

    bool IsScreenLocked() const;

    // Returns whether the session is in an active state.
    bool IsActiveSessionState() const;

    // Returns whether shelf is in auto hide mode and is currently hidden.
    bool IsShelfAutoHidden() const;

    // Returns whether shelf is currently visible.
    bool IsShelfVisible() const;

    // Returns true if the two states are considered equal. As
    // |auto_hide_state| only matters if |visibility_state| is
    // |SHELF_AUTO_HIDE|, Equals() ignores the |auto_hide_state| as
    // appropriate.
    bool Equals(const State& other) const;

    ShelfVisibilityState visibility_state;
    ShelfAutoHideState auto_hide_state;
    WorkspaceWindowState window_state;
    HotseatState hotseat_state;

    // True when the system is in the cancelable, pre-lock screen animation.
    bool pre_lock_screen_animation_active;
    session_manager::SessionState session_state;
  };

  // Sets the visibility of the shelf to |state|.
  void SetState(ShelfVisibilityState visibility_state);

  // Gets the target HotseatState based on the current state of HomeLauncher,
  // Overview, Shelf, and any active gestures.
  HotseatState GetHotseatState(ShelfVisibilityState visibility_state,
                               ShelfAutoHideState auto_hide_state);

  // Returns shelf visibility state based on current value of auto hide
  // behavior setting.
  ShelfVisibilityState CalculateShelfVisibility();

  // Stops any animations and sets the bounds of the shelf and status widgets.
  void LayoutShelfAndUpdateBounds();

  // Updates the bounds and opacity of the shelf and status widgets.
  // If |observer| is specified, it will be called back when the animations, if
  // any, are complete.
  void UpdateBoundsAndOpacity(const TargetBounds& target_bounds,
                              bool animate,
                              ui::ImplicitAnimationObserver* observer);

  // Returns true if a maximized or fullscreen window is being dragged from the
  // top of the display or from the caption area. Note currently for this case
  // it's only allowed in tablet mode, not in laptop mode.
  bool IsDraggingWindowFromTopOrCaptionArea() const;

  // Stops any animations and progresses them to the end.
  void StopAnimating();

  // Calculates shelf target bounds assuming visibility of
  // |state.visibilty_state|.
  void CalculateTargetBounds(const State& state,
                             TargetBounds* target_bounds) const;

  // Calculate the target bounds using |state_|, and updates the
  // |user_work_area_bounds_|.
  void CalculateTargetBoundsAndUpdateWorkArea(TargetBounds* target_bounds);

  // Updates the target bounds if a gesture-drag is in progress. This is only
  // used by |CalculateTargetBounds()|.
  void UpdateTargetBoundsForGesture(TargetBounds* target_bounds) const;

  // Updates the auto hide state immediately.
  void UpdateAutoHideStateNow();

  // Starts the auto hide timer, so that the shelf will be hidden after the
  // timeout (unless something else happens to interrupt / reset it).
  void StartAutoHideTimer();

  // Stops the auto hide timer and clears
  // |mouse_over_shelf_when_auto_hide_timer_started_|.
  void StopAutoHideTimer();

  // Returns the bounds of the shelf on the screen. The returned rect does
  // not include portions of the shelf that extend beyond its own display,
  // as those are not visible to the user.
  gfx::Rect GetVisibleShelfBounds() const;

  // Returns the bounds of an additional region which can trigger showing the
  // shelf. This region exists to make it easier to trigger showing the shelf
  // when the shelf is auto hidden and the shelf is on the boundary between
  // two displays.
  gfx::Rect GetAutoHideShowShelfRegionInScreen() const;

  // Returns the AutoHideState. This value is determined from the shelf and
  // tray.
  ShelfAutoHideState CalculateAutoHideState(
      ShelfVisibilityState visibility_state) const;

  // Returns true if |window| is a descendant of the shelf.
  bool IsShelfWindow(aura::Window* window);

  // Returns true if |window| is a descendant of the status area.
  bool IsStatusAreaWindow(aura::Window* window);

  // Called when the LoginUI changes from visible to invisible.
  void UpdateShelfVisibilityAfterLoginUIChange();

  // Compute |target_bounds| opacity based on gesture and shelf visibility.
  float ComputeTargetOpacity(const State& state) const;

  // Returns true if there is a fullscreen window open that causes the shelf
  // to be hidden.
  bool IsShelfHiddenForFullscreen() const;

  // Returns true if there is a fullscreen or maximized window open that causes
  // the shelf to be autohidden.
  bool IsShelfAutoHideForFullscreenMaximized() const;

  // Returns true if the home gesture handler should handle the event.
  bool ShouldHomeGestureHandleEvent(float scroll_y) const;

  // Gesture drag related functions:
  bool StartGestureDrag(const ui::GestureEvent& gesture_in_screen);
  void UpdateGestureDrag(const ui::GestureEvent& gesture_in_screen);

  // Mouse drag related functions:
  void AttemptToDragByMouse(const ui::MouseEvent& mouse_in_screen);
  void StartMouseDrag(const ui::MouseEvent& mouse_in_screen);
  void UpdateMouseDrag(const ui::MouseEvent& mouse_in_screen);
  void ReleaseMouseDrag(const ui::MouseEvent& mouse_in_screen);

  // Drag related functions, utilized by both gesture drag and mouse drag:
  bool IsDragAllowed() const;
  bool StartAppListDrag(const ui::LocatedEvent& event_in_screen,
                        float scroll_y_hint);
  bool StartShelfDrag(const ui::LocatedEvent& event_in_screen);
  // Sets the Hotseat up to be dragged, if applicable.
  void MaybeSetupHotseatDrag(const ui::LocatedEvent& event_in_screen);
  void UpdateDrag(const ui::LocatedEvent& event_in_screen,
                  float scroll_x,
                  float scroll_y);
  void CompleteDrag(const ui::LocatedEvent& event_in_screen);
  void CompleteAppListDrag(const ui::LocatedEvent& event_in_screen);
  void CancelDrag();

  float GetAppListBackgroundOpacityOnShelfOpacity();

  // Returns true if the gesture is swiping up on a hidden shelf or swiping down
  // on a visible shelf; other gestures should not change shelf visibility.
  bool IsSwipingCorrectDirection();

  // Returns true if should change the visibility of the shelf after drag.
  bool ShouldChangeVisibilityAfterDrag(
      const ui::LocatedEvent& gesture_in_screen);

  // Updates the mask to limit the content to the non lock screen container.
  // The mask will be removed if the workspace state is either in fullscreen
  // or maximized.
  void UpdateWorkspaceMask(WorkspaceWindowState window_state);

  // Sends a11y alert for entering/exiting
  // WorkspaceWindowState::kFullscreen workspace state.
  void SendA11yAlertForFullscreenWorkspaceState(
      WorkspaceWindowState current_workspace_window_state);

  // True when inside UpdateBoundsAndOpacity() method. Used to prevent calling
  // UpdateBoundsAndOpacity() again from SetChildBounds().
  bool updating_bounds_ = false;

  bool in_shutdown_ = false;

  // True if the last mouse event was a mouse drag.
  bool in_mouse_drag_ = false;

  // Current state.
  State state_;

  ShelfWidget* shelf_widget_;
  Shelf* shelf_;

  // Whether the app list is visible. This is maintained by
  // OnAppListVisibilityChanged. Used to determine AppList visibility in
  // clamshell mode.
  bool is_app_list_visible_ = false;

  // Whether the HomeLauncher is shown. This is maintained by
  // OnHomeLauncherAnimationComplete. Used to determine AppList visibility in
  // tablet mode.
  bool is_home_launcher_shown_ = false;

  enum HomeLauncherAnimationState {
    kFinished,
    kShowing,
    kHiding,
  };

  // Whether the home launcher is showing, hiding, or not animating. Maintained
  // by the AppList and HomeLauncher visibility observers.
  HomeLauncherAnimationState home_launcher_animation_state_ = kFinished;

  // Count of pending visibility update suspensions. Skip updating shelf
  // visibility state if it is greater than 0.
  int suspend_visibility_update_ = 0;

  base::OneShotTimer auto_hide_timer_;

  // Whether the mouse was over the shelf when the auto hide timer started.
  // False when neither the auto hide timer nor the timer task are running.
  bool mouse_over_shelf_when_auto_hide_timer_started_ = false;

  // Whether the mouse pointer (not the touch pointer) was over the shelf last
  // time we saw it. This is used to differentiate between mouse and touch in
  // the shelf autohide behavior.
  bool last_seen_mouse_position_was_over_shelf_ = false;

  base::ObserverList<ShelfLayoutManagerObserver>::Unchecked observers_;

  // The enum keeps track of the present status of the drag (from gesture or
  // mouse). The shelf reacts to drags, and can be set to auto-hide for certain
  // events. For example, swiping up from the shelf in tablet mode can open the
  // fullscreen app list. Some shelf behaviour (e.g. visibility state,
  // background color etc.) are affected by various stages of the drag.
  enum DragStatus {
    kDragNone,
    kDragAttempt,
    kDragInProgress,
    kDragCancelInProgress,
    kDragCompleteInProgress,
    kDragAppListInProgress,
  };

  DragStatus drag_status_ = kDragNone;

  // Whether the hotseat is being dragged.
  bool hotseat_is_in_drag_ = false;

  // Whether the EXTENDED hotseat should be hidden. Set when HotseatEventHandler
  // detects that the background has been interacted with.
  bool should_hide_hotseat_ = false;

  // Tracks the amount of the drag. The value is only valid when
  // |drag_status_| is set to kDragInProgress.
  float drag_amount_ = 0.f;

  // The velocity of the last drag event. Used to determine final state of the
  // hotseat.
  int last_drag_velocity_ = 0;

  // Tracks the amount of launcher that above the shelf bottom during dragging.
  float launcher_above_shelf_bottom_amount_ = 0.f;

  // Manage the auto-hide state during drag.
  ShelfAutoHideState drag_auto_hide_state_ = SHELF_AUTO_HIDE_SHOWN;

  // Used to delay updating shelf background.
  UpdateShelfObserver* update_shelf_observer_ = nullptr;

  // Whether background blur is enabled.
  const bool is_background_blur_enabled_;

  // Pretarget handler responsible for hiding the hotseat.
  std::unique_ptr<ui::EventHandler> hotseat_event_handler_;

  // Stores the previous workspace state. Used by
  // SendA11yAlertForFullscreenWorkspaceState to compare with current workspace
  // state to determite whether need to send an a11y alert.
  WorkspaceWindowState previous_workspace_window_state_ =
      WorkspaceWindowState::kDefault;

  // The display on which this shelf is shown.
  display::Display display_;

  // Sets shelf opacity to 0 after all animations have completed.
  std::unique_ptr<ui::ImplicitAnimationObserver> hide_animation_observer_;

  // The current shelf background. Should not be assigned to directly, use
  // MaybeUpdateShelfBackground() instead.
  ShelfBackgroundType shelf_background_type_ = SHELF_BACKGROUND_DEFAULT;

  // Shelf will become transparent if launcher is opened. Stores the shelf
  // background type before open the launcher when start to drag the launcher
  // from shelf.
  ShelfBackgroundType shelf_background_type_before_drag_ =
      SHELF_BACKGROUND_DEFAULT;

  ScopedSessionObserver scoped_session_observer_{this};
  ScopedObserver<WallpaperController, WallpaperControllerObserver>
      wallpaper_controller_observer_{this};

  // Location of the most recent mouse drag event in screen coordinate.
  gfx::Point last_mouse_drag_position_;

  // When it is true, |CalculateAutoHideState| returns the current auto hide
  // state.
  bool is_auto_hide_state_locked_ = false;

  // An optional ScopedSuspendVisibilityUpdate that gets created when suspend
  // visibility update is requested for overview and resets when overview no
  // longer needs it. It is used because OnOverviewModeStarting() and
  // OnOverviewModeStartingAnimationComplete() calls are not balanced.
  base::Optional<ScopedSuspendVisibilityUpdate>
      overview_suspend_visibility_update_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManager);
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_LAYOUT_MANAGER_H_
