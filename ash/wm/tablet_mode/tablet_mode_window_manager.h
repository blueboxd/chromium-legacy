// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_MANAGER_H_
#define ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_MANAGER_H_

#include <stdint.h>

#include <map>
#include <unordered_set>
#include <vector>

#include "ash/ash_export.h"
#include "ash/session/session_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/display/display_observer.h"

namespace aura {
class Window;
}

namespace ash {
class TabletModeController;
class TabletModeWindowState;

namespace wm {
class TabletModeEventHandler;
}

// A window manager which - when created - will force all windows into maximized
// mode. Exception are panels and windows which cannot be maximized.
// Windows which cannot be maximized / resized are centered with a layer placed
// behind the window so that no other windows are visible and/or obscured.
// With the destruction of the manager all windows will be restored to their
// original state.
class ASH_EXPORT TabletModeWindowManager : public aura::WindowObserver,
                                           public display::DisplayObserver,
                                           public OverviewObserver,
                                           public ShellObserver,
                                           public SessionObserver {
 public:
  // This should only be created or deleted by the creator
  // (TabletModeController).
  TabletModeWindowManager();
  ~TabletModeWindowManager() override;

  static aura::Window* GetTopWindow();

  void Init();

  // Stops tracking windows and returns them to their clamshell mode state. Work
  // is done here instead of the destructor because TabletModeController may
  // still need this object alive during shutdown.
  void Shutdown();

  // True if |window| is in |window_state_map_|.
  bool IsTrackingWindow(aura::Window* window);

  // Returns the number of maximized & tracked windows by this manager.
  int GetNumberOfManagedWindows();

  // Adds a window which needs to be maximized. This is used by other window
  // managers for windows which needs to get tracked due to (upcoming) state
  // changes.
  // The call gets ignored if the window was already or should not be handled.
  void AddWindow(aura::Window* window);

  // Called from a window state object when it gets destroyed.
  void WindowStateDestroyed(aura::Window* window);

  // Tell all managing windows not to handle WM events.
  void SetIgnoreWmEventsForExit();

  // OverviewObserver:
  void OnOverviewModeEndingAnimationComplete(bool canceled) override;

  // ShellObserver:
  void OnSplitViewModeEnded() override;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayRemoved(const display::Display& display) override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

 private:
  using WindowToState = std::map<aura::Window*, TabletModeWindowState*>;

  // Returns the state type that |window| had before tablet mode started. If
  // |window| is not yet tracked, returns the current state type of |window|.
  WindowStateType GetDesktopWindowStateType(aura::Window* window) const;

  // Returns a std::vector of up to two split view snap positions, parallel to
  // |windows|, implementing the logic for carrying over snapped window states
  // from desktop mode to tablet mode: if the active window is snapped, then it
  // shall carry over to split view, along with the previous window if it is
  // snapped to the opposite side.
  std::vector<SplitViewController::SnapPosition> GetSnapPositions(
      const MruWindowTracker::WindowList& windows) const;

  // Maximize all windows, except that snapped windows shall carry over to split
  // view as determined by GetSnapPositions().
  void ArrangeWindowsForTabletMode();

  // Revert all windows to how they were arranged before tablet mode.
  // |was_in_overview| indicates whether it was in overview before entering
  // desktop mode.
  void ArrangeWindowsForDesktopMode(bool was_in_overview = false);

  // If the given window should be handled by us, this function will add it to
  // the list of known windows (remembering the initial show state).
  // Note: If the given window cannot be handled by us the function will return
  // immediately.
  void TrackWindow(aura::Window* window,
                   bool entering_tablet_mode = false,
                   bool snap = false,
                   bool animate_bounds_on_attach = true);

  // Remove a window from our tracking list. |was_in_overview| used when
  // |destroyed| is false to help handle leaving tablet mode. If the window is
  // going to be destroyed, do not restore its old previous window state object
  // as it will send unnecessary window state change event.
  void ForgetWindow(aura::Window* window,
                    bool destroyed,
                    bool was_in_overview = false);

  // Returns true when the given window should be modified in any way by us.
  bool ShouldHandleWindow(aura::Window* window);

  // Add window creation observers to track creation of new windows.
  void AddWindowCreationObservers();

  // Remove Window creation observers.
  void RemoveWindowCreationObservers();

  // Change the internal state (e.g. observers) when the display configuration
  // changes.
  void DisplayConfigurationChanged();

  // Returns true when the |window| is a container window.
  bool IsContainerWindow(aura::Window* window);

  // Add a backdrop behind the currently active window on each desktop.
  void EnableBackdropBehindTopWindowOnEachDisplay(bool enable);

  // Every window which got touched by our window manager gets added here.
  WindowToState window_state_map_;

  // All container windows which have to be tracked.
  std::unordered_set<aura::Window*> observed_container_windows_;

  // Windows added to the container, but not yet shown.
  std::unordered_set<aura::Window*> added_windows_;

  // All accounts that have been active at least once since tablet mode started.
  base::flat_set<AccountId> accounts_since_entering_tablet_;

  std::unique_ptr<wm::TabletModeEventHandler> event_handler_;

  DISALLOW_COPY_AND_ASSIGN(TabletModeWindowManager);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_TABLET_MODE_WINDOW_MANAGER_H_
