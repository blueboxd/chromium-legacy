// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/safe_browsing_handler.h"

#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/features.h"

namespace settings {

SafeBrowsingHandler::SafeBrowsingHandler(Profile* profile)
    : profile_(profile) {}
SafeBrowsingHandler::~SafeBrowsingHandler() = default;

void SafeBrowsingHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getSafeBrowsingRadioManagedState",
      base::BindRepeating(
          &SafeBrowsingHandler::HandleGetSafeBrowsingRadioManagedState,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "validateSafeBrowsingEnhanced",
      base::BindRepeating(
          &SafeBrowsingHandler::HandleValidateSafeBrowsingEnhanced,
          base::Unretained(this)));
}

void SafeBrowsingHandler::HandleGetSafeBrowsingRadioManagedState(
    const base::ListValue* args) {
  AllowJavascript();
  CHECK_EQ(1U, args->GetList().size());
  std::string callback_id = args->GetList()[0].GetString();

  auto state = GetSafeBrowsingRadioManagedState(profile_);

  base::Value result(base::Value::Type::DICTIONARY);
  // TODO(crbug.com/1063265): Move managed state functions out of site_settings.
  result.SetKey(kSafeBrowsingEnhanced,
                site_settings::GetValueForManagedState(state.enhanced));
  result.SetKey(kSafeBrowsingStandard,
                site_settings::GetValueForManagedState(state.standard));
  result.SetKey(kSafeBrowsingDisabled,
                site_settings::GetValueForManagedState(state.disabled));

  ResolveJavascriptCallback(base::Value(callback_id), result);
}

void SafeBrowsingHandler::HandleValidateSafeBrowsingEnhanced(
    const base::ListValue* args) {
  // TODO(crbug.com/1074499) Remove this logic when Enhanced protection is
  // considered stable.
  if (!base::FeatureList::IsEnabled(safe_browsing::kEnhancedProtection))
    profile_->GetPrefs()->SetBoolean(prefs::kSafeBrowsingEnhanced, false);
}

SafeBrowsingRadioManagedState
SafeBrowsingHandler::GetSafeBrowsingRadioManagedState(Profile* profile) {
  // Create a default managed state that is updated based on preferences.
  SafeBrowsingRadioManagedState managed_state;

  // Computing the effective Safe Browsing managed state requires inspecting
  // three different preferences. It is possible that these may be in
  // temporarily conflicting managed states. The enabled preference is always
  // taken as the canonical source of management.
  const PrefService::Preference* enabled_pref =
      profile->GetPrefs()->FindPreference(prefs::kSafeBrowsingEnabled);
  const bool enabled_enforced = !enabled_pref->IsUserModifiable();
  const bool enabled_recommended =
      (enabled_pref && enabled_pref->GetRecommendedValue());
  const bool enabled_recommended_on =
      enabled_recommended && enabled_pref->GetRecommendedValue()->GetBool();
  const auto enabled_policy_indicator =
      site_settings::GetPolicyIndicatorFromPref(enabled_pref);

  // The enhanced preference may have a recommended setting. This only takes
  // effect if the enabled preference also has a recommended setting.
  const PrefService::Preference* enhanced_pref =
      profile->GetPrefs()->FindPreference(prefs::kSafeBrowsingEnhanced);
  const bool enhanced_recommended_on =
      enhanced_pref->GetRecommendedValue() &&
      enhanced_pref->GetRecommendedValue()->GetBool();

  // A forcefully disabled reporting preference will disallow enhanced from
  // being selected and thus it must also be considered.
  const PrefService::Preference* reporting_pref =
      profile->GetPrefs()->FindPreference(
          prefs::kSafeBrowsingScoutReportingEnabled);
  const bool reporting_on = reporting_pref->GetValue()->GetBool();
  const bool reporting_enforced = !reporting_pref->IsUserModifiable();
  const auto reporting_policy_indicator =
      site_settings::GetPolicyIndicatorFromPref(reporting_pref);

  if (!enabled_enforced && !enabled_recommended && !reporting_enforced) {
    // No relevant policies are applied, return the default state.
    return managed_state;
  }
  if (enabled_enforced) {
    // All radio controls are managed.
    managed_state.enhanced.disabled = true;
    managed_state.enhanced.indicator = enabled_policy_indicator;
    managed_state.standard.disabled = true;
    managed_state.standard.indicator = enabled_policy_indicator;
    managed_state.disabled.disabled = true;
    managed_state.disabled.indicator = enabled_policy_indicator;
    return managed_state;
  }
  if (enabled_recommended) {
    if (enhanced_recommended_on) {
      managed_state.enhanced.indicator = enabled_policy_indicator;
    } else if (enabled_recommended_on) {
      managed_state.standard.indicator = enabled_policy_indicator;
    } else {
      managed_state.disabled.indicator = enabled_policy_indicator;
    }
    return managed_state;
  }
  if (reporting_enforced && !reporting_on) {
    // Disable enhanced protection when reporting has been enforced off.
    managed_state.enhanced.disabled = true;
    managed_state.enhanced.indicator = reporting_policy_indicator;
    return managed_state;
  }

  return managed_state;
}

}  // namespace settings
