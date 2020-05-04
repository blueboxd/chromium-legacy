// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/marketing_opt_in_screen.h"

#include <cstddef>
#include <string>
#include <unordered_set>

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/tablet_mode.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/user_type_filter.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/marketing_backend_connector.h"
#include "chrome/browser/chromeos/login/screen_manager.h"
#include "chrome/browser/chromeos/login/screens/gesture_navigation_screen.h"
#include "chrome/browser/chromeos/login/users/chrome_user_manager_util.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/gesture_navigation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/marketing_opt_in_screen_handler.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {
MarketingOptInScreen::Country GetCountryFromTimezone(
    const std::string& timezone_id) {
  const size_t buf_size = 8;
  char region[buf_size];
  UErrorCode error = U_ZERO_ERROR;
  icu::UnicodeString str(timezone_id.c_str(), timezone_id.size());
  icu::TimeZone::getRegion(str, region, buf_size, error);
  if (error != 0) {
    LOG(WARNING) << "Could not determine country code. ";
    return MarketingOptInScreen::Country::OTHER;
  }
  const std::string region_str(region);
  if (region_str == "US") {
    return MarketingOptInScreen::Country::US;
  }
  if (region_str == "CA") {
    return MarketingOptInScreen::Country::CA;
  }
  if (region_str == "GB") {
    return MarketingOptInScreen::Country::GB;
  } else {
    return MarketingOptInScreen::Country::OTHER;
  }
}

bool IsDefaultOptInCountry(MarketingOptInScreen::Country country) {
  return (country == MarketingOptInScreen::Country::US);
}

std::string GetCountryCode(MarketingOptInScreen::Country country) {
  switch (country) {
    case MarketingOptInScreen::Country::US:
      return std::string("us");
    case MarketingOptInScreen::Country::CA:
      return std::string("ca");
    case MarketingOptInScreen::Country::GB:
      return std::string("uk");  // Due to server implementation. Not an error.
    case MarketingOptInScreen::Country::OTHER:
      NOTREACHED();
      return std::string("unspecified");
  }
}

// Records the opt-in and opt-out rates for Chromebook emails. Differentiates
// between users who have a default opt-in vs. a default opt-out option.
void RecordOptInAndOptOutRates(const bool user_opted_in,
                               const bool opt_in_by_default) {
  MarketingOptInScreen::Event event;
  if (opt_in_by_default)  // A 'checked' toggle was shown.
    event = (user_opted_in)
                ? MarketingOptInScreen::Event::kUserOptedInWhenDefaultIsOptIn
                : MarketingOptInScreen::Event::kUserOptedOutWhenDefaultIsOptIn;
  else  // An 'unchecked' toggle was shown
    event = (user_opted_in)
                ? MarketingOptInScreen::Event::kUserOptedInWhenDefaultIsOptOut
                : MarketingOptInScreen::Event::kUserOptedOutWhenDefaultIsOptOut;

  base::UmaHistogramEnumeration("OOBE.MarketingOptInScreen.Event", event);
}

}  // namespace

// static
std::string MarketingOptInScreen::GetResultString(Result result) {
  switch (result) {
    case Result::NEXT:
      return "Next";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

MarketingOptInScreen::MarketingOptInScreen(
    MarketingOptInScreenView* view,
    const ScreenExitCallback& exit_callback)
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

// static
MarketingOptInScreen* MarketingOptInScreen::Get(ScreenManager* manager) {
  return static_cast<MarketingOptInScreen*>(
      manager->GetScreen(MarketingOptInScreenView::kScreenId));
}

bool MarketingOptInScreen::MaybeSkip() {
  if (!base::FeatureList::IsEnabled(features::kOobeMarketingScreen) ||
      chrome_user_manager_util::IsPublicSessionOrEphemeralLogin()) {
    exit_callback_.Run(Result::NOT_APPLICABLE);
    return true;
  }

  return false;
}

void MarketingOptInScreen::ShowImpl() {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();

  // Determine the country from the timezone
  country_ = GetCountryFromTimezone(g_browser_process->local_state()->GetString(
      prefs::kSigninScreenTimezone));

  view_->Show();

  /* Hide the marketing opt-in option if:
   *   1) the user is managed. (enterprise-managed, guest, child, supervised)
   * OR
   *   2) The country is not a valid country
   *
   */
  email_opt_in_visible_ =
      !(IsCurrentUserManaged() || (country_ == Country::OTHER));
  view_->SetOptInVisibility(email_opt_in_visible_);

  /**
   * Set the default state of the email opt-in toggle. Geolocation based.
   */
  view_->SetEmailToggleState(IsDefaultOptInCountry(country_));

  // Only show the link for accessibility settings if the gesture navigation
  // screen was shown.
  view_->UpdateA11ySettingsButtonVisibility(
      static_cast<GestureNavigationScreen*>(
          WizardController::default_controller()->screen_manager()->GetScreen(
              GestureNavigationScreenView::kScreenId))
          ->was_shown());

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
  if (is_hidden())
    return;
  active_user_pref_change_registrar_.reset();
  if (view_)
    view_->Hide();
}

void MarketingOptInScreen::OnGetStarted(bool chromebook_email_opt_in) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();

  // UMA Metrics & API call only when the toggle is visible
  if (email_opt_in_visible_) {
    RecordOptInAndOptOutRates(
        chromebook_email_opt_in /*user_opted_in*/,
        IsDefaultOptInCountry(country_) /*opt_in_by_default*/);

    if ((profile != nullptr) && chromebook_email_opt_in) {
      // Call Chromebook Email Service API
      const std::string country_code = GetCountryCode(country_);
      MarketingBackendConnector::UpdateEmailPreferences(profile, country_code);
    }
  }

  exit_callback_.Run(Result::NEXT);
}

void MarketingOptInScreen::SetA11yButtonVisibilityForTest(bool shown) {
  view_->UpdateA11ySettingsButtonVisibility(shown);
}

void MarketingOptInScreen::OnA11yShelfNavigationButtonPrefChanged() {
  view_->UpdateA11yShelfNavigationButtonToggle(
      ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
          ash::prefs::kAccessibilityTabletModeShelfNavigationButtonsEnabled));
}

bool MarketingOptInScreen::IsCurrentUserManaged() {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  if (profile->IsOffTheRecord())
    return false;
  const std::string user_type = apps::DetermineUserType(profile);
  return (user_type != apps::kUserTypeUnmanaged);
}

}  // namespace chromeos
