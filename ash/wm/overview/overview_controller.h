// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_
#define ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/overview/delayed_animation_observer.h"
#include "ash/wm/overview/overview_delegate.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_session.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/window_occlusion_tracker.h"

namespace ash {

// Manages a overview session which displays an overview of all windows and
// allows selecting a window to activate it.
class ASH_EXPORT OverviewController : public OverviewDelegate,
                                      public ::wm::ActivationChangeObserver {
 public:
  OverviewController();
  ~OverviewController() override;

  // Starts/Ends overview with |type|. Returns true if successful (showing
  // overview would be unsuccessful if there are no windows to show). Depending
  // on |type| the enter/exit animation will look different.
  bool StartOverview(OverviewSession::EnterExitOverviewType type =
                         OverviewSession::EnterExitOverviewType::kNormal);
  bool EndOverview(OverviewSession::EnterExitOverviewType type =
                       OverviewSession::EnterExitOverviewType::kNormal);

  // Returns true if overview mode is active.
  bool InOverviewSession() const;

  // Moves the current selection by |increment| items. Positive values of
  // |increment| move the selection forward, negative values move it backward.
  void IncrementSelection(int increment);

  // Accepts current selection if any. Returns true if a selection was made,
  // false otherwise.
  bool AcceptSelection();

  // Called when the overview button tray has been long pressed. Enters
  // splitview mode if the active window is snappable. Also enters overview mode
  // if device is not currently in overview mode.
  // TODO(sammiequon): Move this function to SplitViewController.
  void OnOverviewButtonTrayLongPressed(const gfx::Point& event_location);

  // Returns true if we're in start-overview animation.
  bool IsInStartAnimation();

  // Returns true if overview has been shutdown, but is still animating to the
  // end state ui.
  bool IsCompletingShutdownAnimations();

  // Pause or unpause the occlusion tracker. Resets the unpause delay if we were
  // already in the process of unpausing.
  void PauseOcclusionTracker();
  void UnpauseOcclusionTracker(int delay);

  void AddObserver(OverviewObserver* observer);
  void RemoveObserver(OverviewObserver* observer);

  // Post a task to update the shadow and rounded corners of overview windows.
  void DelayedUpdateRoundedCornersAndShadow();

  // OverviewDelegate:
  void AddExitAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation) override;
  void RemoveAndDestroyExitAnimationObserver(
      DelayedAnimationObserver* animation) override;
  void AddEnterAnimationObserver(
      std::unique_ptr<DelayedAnimationObserver> animation_observer) override;
  void RemoveAndDestroyEnterAnimationObserver(
      DelayedAnimationObserver* animation_observer) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivating(ActivationReason reason,
                          aura::Window* gained_active,
                          aura::Window* lost_active) override;
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {}
  void OnAttemptToReactivateWindow(aura::Window* request_active,
                                   aura::Window* actual_active) override;

  OverviewSession* overview_session() { return overview_session_.get(); }

  void set_occlusion_pause_duration_for_end_ms_for_test(int duration) {
    occlusion_pause_duration_for_end_ms_ = duration;
  }
  void set_delayed_animation_task_delay_for_test(base::TimeDelta delta) {
    delayed_animation_task_delay_ = delta;
  }

  // Returns wallpaper blur status for testing.
  bool HasBlurForTest() const;
  bool HasBlurAnimationForTest() const;

  // Gets the windows list that are shown in the overview windows grids if the
  // overview mode is active for testing.
  std::vector<aura::Window*> GetWindowsListInOverviewGridsForTest();
  std::vector<aura::Window*> GetItemWindowListInOverviewGridsForTest();

 private:
  class OverviewWallpaperController;
  friend class OverviewSessionTest;
  FRIEND_TEST_ALL_PREFIXES(TabletModeControllerTest,
                           DisplayDisconnectionDuringOverview);

  // Attempts to toggle overview mode and returns true if successful (showing
  // overview would be unsuccessful if there are no windows to show). Depending
  // on |type| the enter/exit animation will look different.
  bool ToggleOverview(OverviewSession::EnterExitOverviewType type =
                          OverviewSession::EnterExitOverviewType::kNormal);

  // There is no need to blur or dim the wallpaper for tests.
  static void SetDoNotChangeWallpaperForTests();

  // Returns true if selecting windows in an overview is enabled. This is false
  // at certain times, such as when the lock screen is visible.
  bool CanEnterOverview();

  void OnStartingAnimationComplete(bool canceled);
  void OnEndingAnimationComplete(bool canceled);
  void ResetPauser();

  void UpdateRoundedCornersAndShadow();

  // Collection of DelayedAnimationObserver objects that own widgets that may be
  // still animating after overview mode ends. If shell needs to shut down while
  // those animations are in progress, the animations are shut down and the
  // widgets destroyed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> delayed_animations_;
  // Collection of DelayedAnimationObserver objects. When this becomes empty,
  // notify shell that the starting animations have been completed.
  std::vector<std::unique_ptr<DelayedAnimationObserver>> start_animations_;

  std::unique_ptr<aura::WindowOcclusionTracker::ScopedPause>
      occlusion_tracker_pauser_;

  std::unique_ptr<OverviewSession> overview_session_;
  base::Time last_overview_session_time_;

  int occlusion_pause_duration_for_end_ms_;

  // Handles blurring and dimming of the wallpaper when entering or exiting
  // overview mode. Animates the blurring and dimming if necessary.
  std::unique_ptr<OverviewWallpaperController> overview_wallpaper_controller_;

  base::CancelableOnceClosure reset_pauser_task_;

  // App dragging enters overview right away. This task is used to delay the
  // |OnStartingAnimationComplete| call so that some animations do not make the
  // initial setup less performant.
  base::TimeDelta delayed_animation_task_delay_;

  base::ObserverList<OverviewObserver> observers_;

  base::WeakPtrFactory<OverviewController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OverviewController);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_CONTROLLER_H_
