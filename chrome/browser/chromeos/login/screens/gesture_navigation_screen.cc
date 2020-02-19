// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/gesture_navigation_screen.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/tablet_mode.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"

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
  // TODO(mmourgos): If clamshell mode is enabled and device is detachable, then
  // show the gesture navigation flow.

  AccessibilityManager* accessibility_manager = AccessibilityManager::Get();
  if (chrome_user_manager_util::IsPublicSessionOrEphemeralLogin() ||
      !ash::features::IsHideShelfControlsInTabletModeEnabled() ||
      !ash::TabletMode::Get()->InTabletMode() ||
      accessibility_manager->IsSpokenFeedbackEnabled() ||
      accessibility_manager->IsAutoclickEnabled() ||
      accessibility_manager->IsSwitchAccessEnabled()) {
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
    exit_callback_.Run();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

}  // namespace chromeos
