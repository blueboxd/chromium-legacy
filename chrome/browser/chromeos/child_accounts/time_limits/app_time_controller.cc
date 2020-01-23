// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_whitelist_policy_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace app_time {

// static
bool AppTimeController::ArePerAppTimeLimitsEnabled() {
  return base::FeatureList::IsEnabled(features::kPerAppTimeLimits);
}

bool AppTimeController::IsAppActivityReportingEnabled() {
  return base::FeatureList::IsEnabled(features::kAppActivityReporting);
}

// static
void AppTimeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kPerAppTimeLimitsPolicy);
  registry->RegisterDictionaryPref(prefs::kPerAppTimeLimitsWhitelistPolicy);
}

AppTimeController::AppTimeController(Profile* profile)
    : app_service_wrapper_(std::make_unique<AppServiceWrapper>(profile)),
      app_registry_(
          std::make_unique<AppActivityRegistry>(app_service_wrapper_.get())) {
  DCHECK(profile);

  if (WebTimeLimitEnforcer::IsEnabled())
    web_time_enforcer_ = std::make_unique<WebTimeLimitEnforcer>(this);
  PrefService* pref_service = profile->GetPrefs();
  RegisterProfilePrefObservers(pref_service);
}

AppTimeController::~AppTimeController() = default;

bool AppTimeController::IsExtensionWhitelisted(
    const std::string& extension_id) const {
  NOTIMPLEMENTED();

  return true;
}

void AppTimeController::RegisterProfilePrefObservers(
    PrefService* pref_service) {
  pref_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_registrar_->Init(pref_service);

  // Adds callbacks to observe policy pref changes.
  // Using base::Unretained(this) is safe here because when |pref_registrar_|
  // gets destroyed, it will remove the observers from PrefService.
  pref_registrar_->Add(
      prefs::kPerAppTimeLimitsPolicy,
      base::BindRepeating(&AppTimeController::TimeLimitsPolicyUpdated,
                          base::Unretained(this)));
  pref_registrar_->Add(
      prefs::kPerAppTimeLimitsWhitelistPolicy,
      base::BindRepeating(&AppTimeController::TimeLimitsWhitelistPolicyUpdated,
                          base::Unretained(this)));
}

void AppTimeController::TimeLimitsPolicyUpdated(const std::string& pref_name) {
  NOTIMPLEMENTED();
}

void AppTimeController::TimeLimitsWhitelistPolicyUpdated(
    const std::string& pref_name) {
  DCHECK_EQ(pref_name, prefs::kPerAppTimeLimitsWhitelistPolicy);

  const base::DictionaryValue* policy = pref_registrar_->prefs()->GetDictionary(
      prefs::kPerAppTimeLimitsWhitelistPolicy);

  // Figure out a way to avoid cloning
  AppTimeLimitsWhitelistPolicyWrapper wrapper(policy);

  web_time_enforcer_->OnTimeLimitWhitelistChanged(wrapper);
}

}  // namespace app_time
}  // namespace chromeos
