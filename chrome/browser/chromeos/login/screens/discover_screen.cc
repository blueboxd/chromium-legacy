// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/discover_screen.h"

#include "ash/public/cpp/tablet_mode.h"
#include "base/check.h"
#include "chrome/browser/chromeos/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/discover_screen_handler.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"

namespace chromeos {

namespace {
const char kFinished[] = "finished";
}

DiscoverScreen::DiscoverScreen(DiscoverScreenView* view,
                               const base::RepeatingClosure& exit_callback)
    : BaseScreen(DiscoverScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

DiscoverScreen::~DiscoverScreen() {
  view_->Bind(nullptr);
}

void DiscoverScreen::ShowImpl() {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  if (chrome_user_manager_util::IsPublicSessionOrEphemeralLogin() ||
      !chromeos::quick_unlock::IsPinEnabled(prefs) ||
      chromeos::quick_unlock::IsPinDisabledByPolicy(prefs)) {
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

void DiscoverScreen::HideImpl() {
  view_->Hide();
}

void DiscoverScreen::OnUserAction(const std::string& action_id) {
  // Only honor finish if discover is currently being shown.
  if (action_id == kFinished) {
    exit_callback_.Run();
    return;
  }
  BaseScreen::OnUserAction(action_id);
}

}  // namespace chromeos
