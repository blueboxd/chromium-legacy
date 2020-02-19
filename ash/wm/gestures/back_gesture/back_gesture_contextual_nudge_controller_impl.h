// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_CONTROLLER_IMPL_H_
#define ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_CONTROLLER_IMPL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/back_gesture_contextual_nudge_controller.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/session/session_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/scoped_observer.h"
#include "base/timer/timer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}

namespace ash {

class BackGestureContextualNudgeDelegate;
class BackGestureContextualNudge;

// The class to decide when to show/hide back gesture contextual nudge.
class ASH_EXPORT BackGestureContextualNudgeControllerImpl
    : public SessionObserver,
      public TabletModeObserver,
      public wm::ActivationChangeObserver,
      public BackGestureContextualNudgeController {
 public:
  BackGestureContextualNudgeControllerImpl();
  BackGestureContextualNudgeControllerImpl(
      const BackGestureContextualNudgeControllerImpl&) = delete;
  BackGestureContextualNudgeControllerImpl& operator=(
      const BackGestureContextualNudgeControllerImpl&) = delete;

  ~BackGestureContextualNudgeControllerImpl() override;

  // SessionObserver:
  void OnActiveUserSessionChanged(const AccountId& account_id) override;

  // TabletModeObserver:
  void OnTabletModeStarted() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // BackGestureContextualNudgeController:
  void NavigationEntryChanged(aura::Window* window) override;

 private:
  // Returns true if we can show back gesture contextual nudge ui in current
  // configuration.
  bool CanShowNudge() const;
  void ShowNudgeUi();

  // Starts or stops monitoring windows activation changes to decide if and when
  // to show up the contextual nudge ui.
  void UpdateWindowMonitoring();

  ScopedSessionObserver session_observer_{this};
  ScopedObserver<TabletModeController, TabletModeObserver>
      tablet_mode_observer_{this};

  // The delegate to monitor the current active window's navigation status.
  std::unique_ptr<BackGestureContextualNudgeDelegate> nudge_delegate_;

  // The nudge widget.
  std::unique_ptr<BackGestureContextualNudge> nudge_;

  // True if we're currently monitoring window activation changes.
  bool is_monitoring_windows_ = false;

  // Timer to automatically show nudge ui in 24 hours.
  base::OneShotTimer auto_show_timer_;
};

}  // namespace ash

#endif  // ASH_WM_GESTURES_BACK_GESTURE_BACK_GESTURE_CONTEXTUAL_NUDGE_CONTROLLER_IMPL_H_
