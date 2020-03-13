// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/gesture_navigation_screen.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/tablet_mode.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {

constexpr const char kUserActionExitPressed[] = "exit";

}  // namespace

GestureNavigationScreen::GestureNavigationScreen(
    GestureNavigationScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(GestureNavigationScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

GestureNavigationScreen::~GestureNavigationScreen() {
  if (view_)
    view_->Bind(nullptr);
}

void GestureNavigationScreen::ShowImpl() {
  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (chrome_user_manager_util::IsPublicSessionOrEphemeralLogin() ||
      !ash::features::IsHideShelfControlsInTabletModeEnabled() ||
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled) ||
      accessibility_manager->IsSpokenFeedbackEnabled() ||
      accessibility_manager->IsAutoclickEnabled() ||
      accessibility_manager->IsSwitchAccessEnabled()) {
    exit_callback_.Run();
    return;
  }

  // Skip the screen if the device is not in tablet mode, unless tablet mode
  // first user run is forced on the device.
  if (!ash::TabletMode::Get()->InTabletMode() &&
      !chromeos::switches::ShouldOobeUseTabletModeFirstRun()) {
    exit_callback_.Run();
    return;
  }

  view_->Show();
}

void GestureNavigationScreen::HideImpl() {
  view_->Hide();
}

void GestureNavigationScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionExitPressed) {
    // Make sure the user does not see a notification about the new gestures
    // since they have already gone through this gesture education screen.
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
        ash::prefs::kGestureEducationNotificationShown, true);
    exit_callback_.Run();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

}  // namespace chromeos
