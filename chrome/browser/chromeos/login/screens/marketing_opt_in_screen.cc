// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"

#include "ash/public/cpp/login_screen.h"
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
    : BaseScreen(MarketingOptInScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

MarketingOptInScreen::~MarketingOptInScreen() {
  view_->Bind(nullptr);

  ClearLoginShelfGestureHandler();
}

void MarketingOptInScreen::OnShelfConfigUpdated() {
  bool allow_shelf_gestures = !ash::ShelfConfig::Get()->shelf_controls_shown();
  if (allow_shelf_gestures == handling_shelf_gestures_)
    return;

  if (!allow_shelf_gestures) {
    // handling_shelf_gestures_ will be reset in
    // OnShelfGestureDetectionStopped(), which is called when the handler is
    // cleared.
    ash::LoginScreen::Get()->ClearLoginShelfGestureHandler();
    return;
  }

  handling_shelf_gestures_ =
      ash::LoginScreen::Get()->SetLoginShelfGestureHandler(
          l10n_util::GetStringUTF16(
              IDS_LOGIN_MARKETING_OPT_IN_SCREEN_SWIPE_FROM_SHELF_LABEL),
          base::BindRepeating(&MarketingOptInScreen::HandleFlingFromShelf,
                              weak_factory_.GetWeakPtr()),
          base::BindOnce(&MarketingOptInScreen::OnShelfGestureDetectionStopped,
                         weak_factory_.GetWeakPtr()));

  if (handling_shelf_gestures_)
    view_->UpdateAllSetButtonVisibility(false /*visible*/);
}

void MarketingOptInScreen::ShowImpl() {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  // Skip the screen if:
  //   1) the feature is disabled, or
  //   2) the screen has been shown for this user, or
  //   3) it is public session or non-regular ephemeral user login.
  //    AND
  //   4) the gesture navigation screen was skipped.
  if ((!base::CommandLine::ForCurrentProcess()->HasSwitch(
           chromeos::switches::kEnableMarketingOptInScreen) ||
       prefs->GetBoolean(prefs::kOobeMarketingOptInScreenFinished) ||
       chrome_user_manager_util::IsPublicSessionOrEphemeralLogin()) &&
      GestureNavigationScreen::ShouldSkipGestureNavigationScreen()) {
    exit_callback_.Run();
    return;
  }

  active_ = true;

  view_->Show();
  prefs->SetBoolean(prefs::kOobeMarketingOptInScreenFinished, true);

  shelf_config_observer_.Add(ash::ShelfConfig::Get());
  OnShelfConfigUpdated();

  // Make sure the screen next button visibility is properly initialized.
  view_->UpdateAllSetButtonVisibility(!handling_shelf_gestures_ /*visible*/);
}

void MarketingOptInScreen::HideImpl() {
  if (!active_)
    return;

  active_ = false;

  shelf_config_observer_.RemoveAll();

  view_->Hide();

  ClearLoginShelfGestureHandler();
}

void MarketingOptInScreen::OnAllSet(bool play_communications_opt_in,
                                    bool tips_communications_opt_in) {
  // TODO(https://crbug.com/852557)
  ExitScreen();
}

void MarketingOptInScreen::HandleFlingFromShelf() {
  ExitScreen();
}

void MarketingOptInScreen::OnShelfGestureDetectionStopped() {
  // This is called whenever the shelf gesture handler is cleared - ignore the
  // callback if |handling_shelf_gestures_| was reset before clearing the
  // gesture handler.
  if (!handling_shelf_gestures_)
    return;

  handling_shelf_gestures_ = false;
  view_->UpdateAllSetButtonVisibility(true /*visible*/);
}

void MarketingOptInScreen::ExitScreen() {
  if (!active_)
    return;

  active_ = false;
  ClearLoginShelfGestureHandler();

  exit_callback_.Run();
}

void MarketingOptInScreen::ClearLoginShelfGestureHandler() {
  if (!handling_shelf_gestures_)
    return;

  handling_shelf_gestures_ = false;
  ash::LoginScreen::Get()->ClearLoginShelfGestureHandler();
}

}  // namespace chromeos
