// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge_controller_impl.h"

#include "ash/public/cpp/back_gesture_contextual_nudge_delegate.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/gestures/back_gesture/back_gesture_contextual_nudge.h"
#include "ash/wm/window_util.h"
#include "components/prefs/pref_service.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

PrefService* GetActivePrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

}  // namespace

BackGestureContextualNudgeControllerImpl::
    BackGestureContextualNudgeControllerImpl() {
  tablet_mode_observer_.Add(Shell::Get()->tablet_mode_controller());
}

BackGestureContextualNudgeControllerImpl::
    ~BackGestureContextualNudgeControllerImpl() {
  if (is_monitoring_windows_) {
    nudge_delegate_.reset();
    Shell::Get()->activation_client()->RemoveObserver(this);
  }
}

void BackGestureContextualNudgeControllerImpl::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  UpdateWindowMonitoring();
}

void BackGestureContextualNudgeControllerImpl::OnTabletModeStarted() {
  UpdateWindowMonitoring();

  // If there is an active window at this moment and we should monitor its
  // navigation status, start monitoring it.
  if (is_monitoring_windows_) {
    aura::Window* active_window = window_util::GetActiveWindow();
    if (active_window)
      nudge_delegate_->MaybeStartTrackingNavigation(active_window);
  }
}

void BackGestureContextualNudgeControllerImpl::OnTabletModeEnded() {
  UpdateWindowMonitoring();

  // TODO: Also cancel in-waiting animation or in-progress animation.
}

void BackGestureContextualNudgeControllerImpl::OnTabletControllerDestroyed() {
  tablet_mode_observer_.RemoveAll();
}

void BackGestureContextualNudgeControllerImpl::OnWindowActivated(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!gained_active)
    return;

  // TODO: If another window is activated when the nudge is waiting to be shown
  // or is currently being shown, cancel the animation.

  // Otherwise, start tracking |gained_active|'s navigation status and show the
  // contextual nudge ui if applicable.
  nudge_delegate_->MaybeStartTrackingNavigation(gained_active);
}

void BackGestureContextualNudgeControllerImpl::NavigationEntryChanged(
    aura::Window* window) {
  if (Shell::Get()->shell_delegate()->CanGoBack(window) && CanShowNudge())
    ShowNudgeUi();
}

bool BackGestureContextualNudgeControllerImpl::CanShowNudge() const {
  if (!Shell::Get()->IsInTabletMode())
    return false;

  return contextual_tooltip::ShouldShowNudge(
      GetActivePrefService(), contextual_tooltip::TooltipType::kBackGesture);
}

void BackGestureContextualNudgeControllerImpl::ShowNudgeUi() {
  nudge_ = std::make_unique<BackGestureContextualNudge>();

  // TODO: Only call HandleNudgeShown after |nudge_| animation is done.
  contextual_tooltip::HandleNudgeShown(
      GetActivePrefService(), contextual_tooltip::TooltipType::kBackGesture);
  UpdateWindowMonitoring();

  // Set a timer to monitoring windows and show nudge ui again.
  auto_show_timer_.Start(
      FROM_HERE, contextual_tooltip::kMinInterval, this,
      &BackGestureContextualNudgeControllerImpl::UpdateWindowMonitoring);
}

void BackGestureContextualNudgeControllerImpl::UpdateWindowMonitoring() {
  const bool should_monitor = CanShowNudge();
  if (is_monitoring_windows_ == should_monitor)
    return;
  is_monitoring_windows_ = should_monitor;

  if (should_monitor) {
    // Start monitoring window.
    nudge_delegate_ = Shell::Get()
                          ->shell_delegate()
                          ->CreateBackGestureContextualNudgeDelegate(this);
    Shell::Get()->activation_client()->AddObserver(this);
    return;
  }

  // Stop monitoring window.
  nudge_delegate_.reset();
  Shell::Get()->activation_client()->RemoveObserver(this);
}

}  // namespace ash
