// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/chromeos/login/screens/gesture_navigation_screen.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

MarketingOptInScreen::MarketingOptInScreen(
    MarketingOptInScreenView* view,
    const base::RepeatingClosure& exit_callback)
    : BaseScreen(MarketingOptInScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

MarketingOptInScreen::~MarketingOptInScreen() {
  view_->Bind(nullptr);
}

void MarketingOptInScreen::ShowImpl() {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();

  const bool did_skip_gesture_navigation_screen =
      !prefs->GetBoolean(ash::prefs::kGestureEducationNotificationShown);

  // Always skip the screen if it is a public session or non-regular ephemeral
  // user login. Also skip the screen if clamshell mode is active.
  // TODO(mmourgos): Enable this screen for clamshell mode.
  if (chrome_user_manager_util::IsPublicSessionOrEphemeralLogin() ||
      did_skip_gesture_navigation_screen) {
    exit_callback_.Run();
    return;
  }

  // Skip the screen if:
  //   1) the feature is disabled, or
  //   2) the screen has been shown for this user
  //    AND
  //   3) the hide shelf controls in tablet mode feature is disabled.
  if ((!base::CommandLine::ForCurrentProcess()->HasSwitch(
           chromeos::switches::kEnableMarketingOptInScreen) ||
       prefs->GetBoolean(prefs::kOobeMarketingOptInScreenFinished)) &&
      !ash::features::IsHideShelfControlsInTabletModeEnabled()) {
    exit_callback_.Run();
    return;
  }

  active_ = true;

  view_->Show();
  prefs->SetBoolean(prefs::kOobeMarketingOptInScreenFinished, true);

  view_->UpdateA11yShelfNavigationButtonToggle(prefs->GetBoolean(
      ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled));

  // Observe the a11y shelf navigation buttons pref so the setting toggle in the
  // screen can be updated if the pref value changes.
  active_user_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  active_user_pref_change_registrar_->Init(prefs);
  active_user_pref_change_registrar_->Add(
      ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled,
      base::BindRepeating(
          &MarketingOptInScreen::OnA11yShelfNavigationButtonPrefChanged,
          base::Unretained(this)));
}

void MarketingOptInScreen::HideImpl() {
  if (!active_)
    return;

  active_ = false;
  active_user_pref_change_registrar_.reset();
  view_->Hide();
}

void MarketingOptInScreen::OnAllSet(bool play_communications_opt_in,
                                    bool tips_communications_opt_in) {
  // TODO(https://crbug.com/852557)
  ExitScreen();
}

void MarketingOptInScreen::ExitScreen() {
  if (!active_)
    return;

  active_ = false;

  exit_callback_.Run();
}

void MarketingOptInScreen::OnA11yShelfNavigationButtonPrefChanged() {
  view_->UpdateA11yShelfNavigationButtonToggle(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled));
}

}  // namespace chromeos
