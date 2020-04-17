// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cookie_controls/cookie_controls_service.h"
#include "components/content_settings/core/common/cookie_controls_enforcement.h"

#include <utility>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "url/gurl.h"

CookieControlsService::CookieControlsService(Profile* profile)
    : profile_(profile) {
  Init();
}

CookieControlsService::~CookieControlsService() = default;

void CookieControlsService::Init() {
  pref_change_registrar_.Init(profile_->GetPrefs());
  pref_change_registrar_.Add(
      prefs::kCookieControlsMode,
      base::Bind(&CookieControlsService::OnThirdPartyCookieBlockingPrefChanged,
                 base::Unretained(this)));
  pref_change_registrar_.Add(
      prefs::kBlockThirdPartyCookies,
      base::Bind(&CookieControlsService::OnThirdPartyCookieBlockingPrefChanged,
                 base::Unretained(this)));
  policy_registrar_ = std::make_unique<policy::PolicyChangeRegistrar>(
      profile_->GetProfilePolicyConnector()->policy_service(),
      policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
  policy_registrar_->Observe(
      policy::key::kBlockThirdPartyCookies,
      base::BindRepeating(
          &CookieControlsService::OnThirdPartyCookieBlockingPolicyChanged,
          base::Unretained(this)));
}

void CookieControlsService::Shutdown() {
  pref_change_registrar_.RemoveAll();
  policy_registrar_.reset();
}

void CookieControlsService::HandleCookieControlsToggleChanged(bool checked) {
  profile_->GetPrefs()->SetInteger(
      prefs::kCookieControlsMode,
      static_cast<int>(
          checked ? content_settings::CookieControlsMode::kIncognitoOnly
                  : content_settings::CookieControlsMode::kOff));
  base::RecordAction(
      checked ? base::UserMetricsAction("CookieControls.NTP.Enabled")
              : base::UserMetricsAction("CookieControls.NTP.Disabled"));
}

bool CookieControlsService::ShouldHideCookieControlsUI() {
  return !base::FeatureList::IsEnabled(
      content_settings::kImprovedCookieControls);
}

bool CookieControlsService::ShouldEnforceCookieControls() {
  return GetCookieControlsEnforcement() !=
         CookieControlsEnforcement::kNoEnforcement;
}

CookieControlsEnforcement
CookieControlsService::GetCookieControlsEnforcement() {
  auto* pref = profile_->GetPrefs()->FindPreference(prefs::kCookieControlsMode);
  if (pref->IsManaged())
    return CookieControlsEnforcement::kEnforcedByPolicy;
  if (pref->IsExtensionControlled())
    return CookieControlsEnforcement::kEnforcedByExtension;
  if (profile_->GetPrefs()->GetBoolean(prefs::kBlockThirdPartyCookies))
    return CookieControlsEnforcement::kEnforcedByCookieSetting;
  return CookieControlsEnforcement::kNoEnforcement;
}

bool CookieControlsService::GetToggleCheckedValue() {
  return CookieSettingsFactory::GetForProfile(profile_)
      ->ShouldBlockThirdPartyCookies();
}

void CookieControlsService::OnThirdPartyCookieBlockingPrefChanged() {
  for (Observer& obs : observers_)
    obs.OnThirdPartyCookieBlockingPrefChanged();
}

void CookieControlsService::OnThirdPartyCookieBlockingPolicyChanged(
    const base::Value* previous,
    const base::Value* current) {
  for (Observer& obs : observers_)
    obs.OnThirdPartyCookieBlockingPolicyChanged();
}
