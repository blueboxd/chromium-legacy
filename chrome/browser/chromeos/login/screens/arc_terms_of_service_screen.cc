// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/arc_terms_of_service_screen.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/prefs/pref_service.h"

namespace {

constexpr char kUserActionAccepted[] = "accepted";
constexpr char kUserActionBack[] = "go-back";

}  // namespace

namespace chromeos {

// static
std::string ArcTermsOfServiceScreen::GetResultString(Result result) {
  switch (result) {
    case Result::ACCEPTED:
      return "Accepted";
    case Result::BACK:
      return "Back";
  }
}

// static
void ArcTermsOfServiceScreen::MaybeLaunchArcSettings(Profile* profile) {
  if (profile->GetPrefs()->GetBoolean(prefs::kShowArcSettingsOnSessionStart)) {
    profile->GetPrefs()->ClearPref(prefs::kShowArcSettingsOnSessionStart);
    // TODO(jhorwich) Handle the case where the user chooses to review both ARC
    // settings and sync settings - currently the Settings window will only
    // show one settings page. See crbug.com/901184#c4 for details.
    chrome::SettingsWindowManager::GetInstance()->ShowOSSettings(
        profile, chromeos::settings::mojom::kGooglePlayStoreSubpagePath);
  }
}

ArcTermsOfServiceScreen::ArcTermsOfServiceScreen(
    ArcTermsOfServiceScreenView* view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ArcTermsOfServiceScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  if (view_) {
    view_->AddObserver(this);
    view_->Bind(this);
  }
}

ArcTermsOfServiceScreen::~ArcTermsOfServiceScreen() {
  if (view_) {
    view_->RemoveObserver(this);
    view_->Bind(nullptr);
  }
}

void ArcTermsOfServiceScreen::ShowImpl() {
  if (!view_)
    return;

  // Show the screen.
  view_->Show();
}

void ArcTermsOfServiceScreen::HideImpl() {
  if (view_)
    view_->Hide();
}

void ArcTermsOfServiceScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionAccepted) {
    exit_callback_.Run(Result::ACCEPTED);
  } else if (action_id == kUserActionBack) {
    exit_callback_.Run(Result::BACK);
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}

void ArcTermsOfServiceScreen::OnAccept(bool review_arc_settings) {
  if (is_hidden())
    return;
  base::UmaHistogramBoolean("OOBE.ArcTermsOfServiceScreen.ReviewFollowingSetup",
                            review_arc_settings);
  if (review_arc_settings) {
    Profile* const profile = ProfileManager::GetActiveUserProfile();
    CHECK(profile);
    profile->GetPrefs()->SetBoolean(prefs::kShowArcSettingsOnSessionStart,
                                    true);
  }
  HandleUserAction(kUserActionAccepted);
}

void ArcTermsOfServiceScreen::OnViewDestroyed(
    ArcTermsOfServiceScreenView* view) {
  DCHECK_EQ(view, view_);
  view_->RemoveObserver(this);
  view_ = nullptr;
}

}  // namespace chromeos
