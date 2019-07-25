// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITSVIEW_SPLIT_VIEW_CONTROLLER_H_
#define ASH_WM_SPLITSVIEW_SPLIT_VIEW_CONTROLLER_H_

#include <memory>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/split_view.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state_observer.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ui {
class Layer;
}  // namespace ui

namespace ash {
class PresentationTimeRecorder;
class OverviewSession;
class SplitViewControllerTest;
class SplitViewDivider;
class SplitViewOverviewSessionTest;

// The controller for the split view. It snaps a window to left/right side of
// the screen. It also observes the two snapped windows and decides when to exit
// the split view mode.
// TODO(xdai): Make it work for multi-display non mirror environment.
class ASH_EXPORT SplitViewController : public SplitViewNotifier,
                                       public aura::WindowObserver,
                                       public WindowStateObserver,
                                       public wm::ActivationChangeObserver,
                                       public ShellObserver,
                                       public OverviewObserver,
                                       public display::DisplayObserver,
                                       public TabletModeObserver,
                                       public AccessibilityObserver {
 public:
  // "LEFT" and "RIGHT" are the snap positions corresponding to "primary
  // landscape" screen orientation. In other screen orientation, we still use
  // "LEFT" and "RIGHT" but it doesn't literally mean left side of the screen or
  // right side of the screen. For example, if the screen orientation is
  // "portait primary", snapping a window to LEFT means snapping it to the
  // top of the screen.
  enum SnapPosition { NONE, LEFT, RIGHT };

  // Why splitview was ended.
  enum class EndReason {
    kNormal = 0,
    kHomeLauncherPressed,
    kUnsnappableWindowActivated,
    kActiveUserChanged,
    kWindowDragStarted,
    // TODO(edcourtney): Consider not ending Split-View on PIP expand.
    // See crbug.com/950827.
    kPipExpanded,
    kExitTabletMode,
    // Splitview is being ended due to a change in Virtual Desks, such as
    // switching desks or removing a desk.
    kDesksChange,
  };

  // The behaviors of split view are very different when in tablet mode and in
  // clamshell mode. In tablet mode, split view mode will stay active until the
  // user explicitly ends it (e.g., by pressing home launcher, or long pressing
  // the overview button, or sliding the divider bar to the edge, etc). However,
  // in clamshell mode, there is no divider bar, and split view mode only stays
  // active during overview snapping, i.e., it's only possible that split view
  // is active when overview is active. Once the user has selected two windows
  // to snap to both side of the screen, split view mode is no longer active.
  enum class SplitViewType {
    kTabletType = 0,
    kClamshellType,
  };

  SplitViewController();
  ~SplitViewController() override;

  // Returns true if split view mode is active. Please see SplitViewType above
  // to see the difference between tablet mode and clamshell mode splitview
  // mode.
  bool InSplitViewMode() const;
  bool InClamshellSplitViewMode() const;
  bool InTabletSplitViewMode() const;

  // Snaps window to left/right. It will try to remove |window| from the
  // overview window grid first before snapping it if |window| is currently
  // showing in the overview window grid. If split view mode is not already
  // active, and if |window| is not minimized, |use_divider_spawn_animation|
  // causes the divider to show up with an animation that adds a finishing touch
  // to the snap animation of |window|. Use true when |window| is snapped by
  // dragging, except for tab dragging.
  void SnapWindow(aura::Window* window,
                  SnapPosition snap_position,
                  bool use_divider_spawn_animation = false);

  // Swaps the left and right windows. This will do nothing if one of the
  // windows is not snapped.
  void SwapWindows();

  // Returns the default snapped window. It's the window that remains open until
  // the split mode ends. It's decided by |default_snap_position_|. E.g., If
  // |default_snap_position_| equals LEFT, then the default snapped window is
  // |left_window_|. All the other window will open on the right side.
  aura::Window* GetDefaultSnappedWindow();

  // Gets the window bounds according to the snap state |snap_state| and the
  // divider position |divider_position_|. The returned snapped window bounds
  // are adjusted to its minimum size if the desired bounds are smaller than
  // its minumum bounds. Note: the snapped window bounds can't be pushed
  // outside of the workspace area.
  // TODO(xdai): Investigate if we need to pass in |window| as splitview windows
  // should always be parented to the primary root window.
  gfx::Rect GetSnappedWindowBoundsInParent(aura::Window* window,
                                           SnapPosition snap_position);
  gfx::Rect GetSnappedWindowBoundsInScreen(aura::Window* window,
                                           SnapPosition snap_position);

  // Gets the desired snapped window bounds accoridng to the snap state
  // |snap_state| and the divider pistion |divider_position_|.
  gfx::Rect GetSnappedWindowBoundsInScreenUnadjusted(aura::Window* window,
                                                     SnapPosition snap_postion);

  // Gets the default value of |divider_position_|.
  int GetDefaultDividerPosition(aura::Window* window) const;

  // Returns true during the divider snap animation.
  bool IsDividerAnimating();

  void StartResize(const gfx::Point& location_in_screen);
  void Resize(const gfx::Point& location_in_screen);
  void EndResize(const gfx::Point& location_in_screen);

  // Ends the split view mode.
  void EndSplitView(EndReason end_reason = EndReason::kNormal);

  // Returns true if |window| is a snapped window in splitview.
  bool IsWindowInSplitView(const aura::Window* window) const;

  // This function is only supposed to be called during clamshell <-> tablet
  // transition or multi-user transition, when we need to carry over one/two
  // snapped windows into splitview, we calculate the divider position based on
  // the one or two to-be-snapped windows' bounds so that we can keep the
  // snapped windows' bounds after transition (instead of putting them always
  // on the middle split position).
  void InitDividerPositionForTransition(int divider_position);

  // Called when a window (either it's browser window or an app window) start/
  // end being dragged.
  void OnWindowDragStarted(aura::Window* dragged_window);
  void OnWindowDragEnded(aura::Window* dragged_window,
                         SnapPosition desired_snap_position,
                         const gfx::Point& last_location_in_screen);
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnResizeLoopStarted(aura::Window* window) override;
  void OnResizeLoopEnded(aura::Window* window) override;

  // SplitViewNotifier:
  SplitViewState GetCurrentState() const override;
  void AddObserver(SplitViewObserver* observer) override;
  void RemoveObserver(SplitViewObserver* observer) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(ash::WindowState* window_state,
                                   ash::WindowStateType old_type) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // ShellObserver:
  void OnPinnedStateChanged(aura::Window* pinned_window) override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // TabletModeObserver:
  void OnTabletModeStarting() override;
  void OnTabletModeStarted() override;
  void OnTabletModeEnding() override;
  void OnTabletControllerDestroyed() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  aura::Window* left_window() { return left_window_; }
  aura::Window* right_window() { return right_window_; }
  int divider_position() const { return divider_position_; }
  SplitViewState state() const { return state_; }
  SnapPosition default_snap_position() const { return default_snap_position_; }
  SplitViewDivider* split_view_divider() { return split_view_divider_.get(); }
  bool is_resizing() const { return is_resizing_; }
  EndReason end_reason() const { return end_reason_; }

 private:
  friend class SplitViewControllerTest;
  friend class SplitViewOverviewSessionTest;
  class TabDraggedWindowObserver;
  class DividerSnapAnimation;

  // Start observing |window|.
  void StartObserving(aura::Window* window);
  // Stop observing the window at associated with |snap_position|. Also updates
  // shadows and sets |left_window_| or |right_window_| to nullptr.
  void StopObserving(SnapPosition snap_position);

  // Update split view state and notify its observer about the change.
  void UpdateSplitViewStateAndNotifyObservers();

  // Notifies observers that the split view divider position has been changed.
  void NotifyDividerPositionChanged();

  // Updates the black scrim layer's bounds and opacity while dragging the
  // divider. The opacity increases as the split divider gets closer to the edge
  // of the screen.
  void UpdateBlackScrim(const gfx::Point& location_in_screen);

  // Updates the bounds for the snapped windows and divider according to the
  // current snap direction.
  void UpdateSnappedWindowsAndDividerBounds();

  // Gets the position where the black scrim should show.
  SnapPosition GetBlackScrimPosition(const gfx::Point& location_in_screen);

  // Updates |divider_position_| according to the current event location during
  // resizing.
  void UpdateDividerPosition(const gfx::Point& location_in_screen);

  // Get the window bounds for left_or_top and right_or_bottom snapped windows.
  // Note the bounds returned by this function doesn't take the snapped windows
  // minimum sizes into account.
  void GetSnappedWindowBoundsInScreenInternal(aura::Window* window,
                                              gfx::Rect* left_or_top_rect,
                                              gfx::Rect* right_or_bottom_rect);

  // Splits the |work_area_rect| by |divider_rect| and outputs the two halves.
  // |left_or_top_rect|, |divider_rect| and |right_or_bottom_rect| should align
  // vertically or horizontally depending on |is_split_vertically|.
  void SplitRect(const gfx::Rect& work_area_rect,
                 const gfx::Rect& divider_rect,
                 const bool is_split_vertically,
                 gfx::Rect* left_or_top_rect,
                 gfx::Rect* right_or_bottom_rect);

  // Returns the closest fix location for |divider_position_|.
  int GetClosestFixedDividerPosition();

  // While the divider is animating to somewhere, stop it and shove it there.
  void StopAndShoveAnimatedDivider();

  // Returns true if we should end split view mode after resizing, i.e., the
  // split view divider is near to the edge of the screen.
  bool ShouldEndSplitViewAfterResizing();

  // Ends split view if ShouldEndSplitViewAfterResizing() returns true. Handles
  // extra details associated with dragging the divider off the screen.
  void EndSplitViewAfterResizingIfAppropriate();

  // After resizing, if we should end split view mode, returns the window that
  // needs to be activated. Returns nullptr if there is no such window.
  aura::Window* GetActiveWindowAfterResizingUponExit();

  // Returns the maximum value of the |divider_position_|. It is the width of
  // the current display's work area bounds in landscape orientation, or height
  // of the current display's work area bounds in portrait orientation.
  int GetDividerEndPosition();

  // Called after a to-be-snapped window |window| got snapped. It updates the
  // split view states and notifies observers about the change. It also restore
  // the snapped window's transform if it's not identity and activate it.
  void OnWindowSnapped(aura::Window* window);

  // If there are two snapped windows, closing/minimizing/tab-dragging one of
  // them will open overview window grid on the closed/minimized/tab-dragged
  // window side of the screen. If there is only one snapped windows, closing/
  // minimizing/tab-dragging the sanpped window will end split view mode and
  // adjust the overview window grid bounds if the overview mode is active at
  // that moment. |window_drag| is true if the window was detached as a result
  // of dragging.
  void OnSnappedWindowDetached(aura::Window* window, bool window_drag);

  // If the desired bounds of the snapped windows bounds |left_or_top_rect| and
  // |right_or_bottom_rect| are smaller than the minimum bounds of the snapped
  // windows, adjust the desired bounds to the minimum bounds. Note the snapped
  // windows can't be pushed out of the work area display area.
  void AdjustSnappedWindowBounds(gfx::Rect* left_or_top_rect,
                                 gfx::Rect* right_or_bottom_rect);

  // Returns the closest position ratio based on |distance| and |length|.
  float FindClosestPositionRatio(float distance, float length);

  // Gets the divider optional position ratios. The divider can always be
  // moved to the positions in |kFixedPositionRatios|. Whether the divider can
  // be moved to |kOneThirdPositionRatio| or |kTwoThirdPositionRatio| depends
  // on the minimum size of current snapped windows.
  void GetDividerOptionalPositionRatios(
      std::vector<float>* out_position_ratios);

  // Gets the expected window component depending on current screen orientation
  // for resizing purpose.
  int GetWindowComponentForResize(aura::Window* window);
  // Gets the expected end drag position for |window| depending on current
  // screen orientation and split divider position.
  gfx::Point GetEndDragLocationInScreen(aura::Window* window,
                                        const gfx::Point& location_in_screen);

  // Restores |window| transform to identity transform if applicable.
  void RestoreTransformIfApplicable(aura::Window* window);

  // Activates the snapped window |window| and stacks the other snapped window
  // below |window| so that the two snapped windows are always the top two
  // windows when split view mode is active.
  void ActivateAndStackSnappedWindow(aura::Window* window);

  // During resizing, it's possible that the resizing bounds of the snapped
  // window is smaller than its minimum bounds, in this case we apply a
  // translation to the snapped window to make it visually be placed outside of
  // the workspace area.
  void SetWindowsTransformDuringResizing();

  // Restore the snapped windows transform to identity transform after resizing.
  void RestoreWindowsTransformAfterResizing();

  // Animates to |target_transform| for |window| and its transient descendants.
  // |window| will be applied |start_transform| first and then animate to
  // |target_transform|. Note |start_transform| and |end_transform| are for
  // |window| and need to be adjusted for its transient child windows.
  void SetTransformWithAnimation(aura::Window* window,
                                 const gfx::Transform& start_transform,
                                 const gfx::Transform& target_transform);

  // Removes the window item that contains |window| from the overview window
  // grid if |window| is currently showing in overview window grid. It should be
  // called before trying to snap the window.
  void RemoveWindowFromOverviewIfApplicable(aura::Window* window);

  // Updates the |snapping_window_transformed_bounds_map_| on |window|. It
  // should be called before trying to snap the window.
  void UpdateSnappingWindowTransformedBounds(aura::Window* window);

  // Inserts |window| into overview window grid if overview mode is active. Do
  // nothing if overview mode is inactive at the moment.
  void InsertWindowToOverview(aura::Window* window, bool animate = true);

  // Finalizes and cleans up after stopping dragging the divider bar to resize
  // snapped windows.
  void FinishWindowResizing(aura::Window* window);

  // Finalizes and cleans up divider dragging/animating. Called when the divider
  // snapping animation completes or is interrupted or totally skipped.
  void EndResizeImpl();

  // Called by OnWindowDragEnded to do the actual work of finishing the window
  // dragging. If |is_being_destroyed| equals true, the dragged window is to be
  // destroyed, and SplitViewController should not try to put it in splitview.
  void EndWindowDragImpl(aura::Window* window,
                         bool is_being_destroyed,
                         SnapPosition desired_snap_position,
                         const gfx::Point& last_location_in_screen);

  // Gets the snap position of |window| according to last mouse/gesture event
  // location on |window|. Used when |desired_snap_position_| was NONE but
  // SplitViewController needs to snap the |window| after dragging.
  SplitViewController::SnapPosition GetSnapPosition(
      aura::Window* window,
      const gfx::Point& last_location_in_screen);

  // The current left/right snapped window.
  aura::Window* left_window_ = nullptr;
  aura::Window* right_window_ = nullptr;

  // Split view divider widget. Only exist in tablet splitview mode. It's a
  // black bar stretching from one edge of the screen to the other, containing a
  // small white drag bar in the middle. As the user presses on it and drag it
  // to left or right, the left and right window will be resized accordingly.
  std::unique_ptr<SplitViewDivider> split_view_divider_;

  // A black scrim layer that fades in over a window when it’s width drops under
  // 1/3 of the width of the screen, increasing in opacity as the divider gets
  // closer to the edge of the screen.
  std::unique_ptr<ui::Layer> black_scrim_layer_;

  // The window observer that obseves the tab-dragged window in tablet mode.
  std::unique_ptr<TabDraggedWindowObserver> dragged_window_observer_;

  // The distance between the origin of the divider and the origin of the
  // current display's work area in screen coordinates.
  //     |<---     divider_position_    --->|
  //     ----------------------------------------------------------
  //     |                                  | |                    |
  //     |        left_window_              | |   right_window_    |
  //     |                                  | |                    |
  //     ----------------------------------------------------------
  int divider_position_ = -1;

  // The closest position ratio of divider among kFixedPositionRatios,
  // kOneThirdPositionRatio and kTwoThirdPositionRatio based on current
  // |divider_position_|. Used to update |divider_position_| on work area
  // changes.
  float divider_closest_ratio_ = 0.f;

  // The location of the previous mouse/gesture event in screen coordinates.
  gfx::Point previous_event_location_;

  // The animation that animates the divider to a fixed position after resizing.
  std::unique_ptr<DividerSnapAnimation> divider_snap_animation_;

  // Current snap state.
  SplitViewState state_ = SplitViewState::kNoSnap;

  // The default snap position. It's decided by the first snapped window. If the
  // first window was snapped left, then |default_snap_position_| equals LEFT,
  // i.e., all the other windows will open snapped on the right side - and vice
  // versa.
  SnapPosition default_snap_position_ = NONE;

  // The previous orientation of the screen.
  OrientationLockType previous_screen_orientation_ = OrientationLockType::kAny;

  // True when the divider is being dragged (not during its snap animation).
  bool is_resizing_ = false;

  // Stores the reason which cause splitview to end.
  EndReason end_reason_ = EndReason::kNormal;

  // The split view type. See SplitViewType for the differences between tablet
  // split view and clamshell split view.
  SplitViewType split_view_type_ = SplitViewType::kTabletType;

  // The time when splitview starts. Used for metric collection purpose.
  base::Time splitview_start_time_;

  // The map from a to-be-snapped window to its transformed bounds.
  base::flat_map<aura::Window*, gfx::Rect>
      snapping_window_transformed_bounds_map_;

  base::ObserverList<SplitViewObserver>::Unchecked observers_;

  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};

  // Records the presentation time of resize operation in split view mode.
  std::unique_ptr<PresentationTimeRecorder> presentation_time_recorder_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewController);
};

}  // namespace ash

#endif  // ASH_WM_SPLITSVIEW_SPLIT_VIEW_CONTROLLER_H_
