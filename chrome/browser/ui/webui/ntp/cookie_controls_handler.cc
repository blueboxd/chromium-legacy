// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/cookie_controls_handler.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/values.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"

CookieControlsHandler::CookieControlsHandler() {}

CookieControlsHandler::~CookieControlsHandler() {}

void CookieControlsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "cookieControlsToggleChanged",
      base::BindRepeating(
          &CookieControlsHandler::HandleCookieControlsToggleChanged,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "observeCookieControlsSettingsChanges",
      base::BindRepeating(
          &CookieControlsHandler::HandleObserveCookieControlsSettingsChanges,
          base::Unretained(this)));
}

void CookieControlsHandler::OnJavascriptAllowed() {
  Profile* profile = Profile::FromWebUI(web_ui());
  pref_change_registrar_.Init(profile->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kCookieControlsMode,
      base::Bind(&CookieControlsHandler::OnCookieControlsChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kBlockThirdPartyCookies,
      base::Bind(&CookieControlsHandler::OnThirdPartyCookieBlockingChanged,
                 base::Unretained(this)));
  policy_registrar_ = std::make_unique<policy::PolicyChangeRegistrar>(
      profile->GetProfilePolicyConnector()->policy_service(),
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  policy_registrar_->Observe(
      policy::key::kBlockThirdPartyCookies,
      base::BindRepeating(
          &CookieControlsHandler::OnThirdPartyCookieBlockingPolicyChanged,
          base::Unretained(this)));
}

void CookieControlsHandler::OnJavascriptDisallowed() {
  pref_change_registrar_.RemoveAll();
  policy_registrar_.reset();
}

void CookieControlsHandler::HandleCookieControlsToggleChanged(
    const base::ListValue* args) {
  bool checked;
  CHECK(args->GetBoolean(0, &checked));
  Profile* profile = Profile::FromWebUI(web_ui());
  profile->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(
          checked ? content_settings::CookieControlsMode::kIncognitoOnly
                  : content_settings::CookieControlsMode::kOff));
  base::RecordAction(
      checked ? base::UserMetricsAction("CookieControls.NTP.Enabled")
              : base::UserMetricsAction("CookieControls.NTP.Disabled"));
}

void CookieControlsHandler::HandleObserveCookieControlsSettingsChanges(
    const base::ListValue* args) {
  AllowJavascript();
  OnCookieControlsChanged();
  OnThirdPartyCookieBlockingChanged();
}

void CookieControlsHandler::OnCookieControlsChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
  FireWebUIListener("cookie-controls-changed",
                    base::Value(GetToggleCheckedValue(profile)));
}

bool CookieControlsHandler::GetToggleCheckedValue(const Profile* profile) {
  int mode = profile->GetPrefs()->GetInteger(prefs::kCookieControlsMode);
  return mode != static_cast<int>(content_settings::CookieControlsMode::kOff);
}

void CookieControlsHandler::OnThirdPartyCookieBlockingChanged() {
  Profile* profile = Profile::FromWebUI(web_ui());
  FireWebUIListener("third-party-cookie-blocking-changed",
                    base::Value(ShouldHideCookieControlsUI(profile)));
}

void CookieControlsHandler::OnThirdPartyCookieBlockingPolicyChanged(
    const base::Value* previous,
    const base::Value* current) {
  OnThirdPartyCookieBlockingChanged();
}

bool CookieControlsHandler::ShouldHideCookieControlsUI(const Profile* profile) {
  return !base::FeatureList::IsEnabled(
             content_settings::kImprovedCookieControls) ||
         profile->GetPrefs()->IsManagedPreference(
             prefs::kBlockThirdPartyCookies) ||
         profile->GetPrefs()->GetBoolean(prefs::kBlockThirdPartyCookies);
}
