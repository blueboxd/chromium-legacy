// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_controller.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/optional.h"
#include "base/values.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_activity_registry.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_service_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_limits_whitelist_policy_wrapper.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_time_policy_helpers.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/app_types.h"
#include "chrome/browser/chromeos/child_accounts/time_limits/web_time_limit_enforcer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace app_time {

AppTimeController::TestApi::TestApi(AppTimeController* controller)
    : controller_(controller) {}

AppTimeController::TestApi::~TestApi() = default;

AppActivityRegistry* AppTimeController::TestApi::app_registry() {
  return controller_->app_registry_.get();
}

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
          std::make_unique<AppActivityRegistry>(app_service_wrapper_.get(),
                                                this)) {
  DCHECK(profile);

  if (WebTimeLimitEnforcer::IsEnabled())
    web_time_enforcer_ = std::make_unique<WebTimeLimitEnforcer>(this);

  PrefService* pref_service = profile->GetPrefs();
  RegisterProfilePrefObservers(pref_service);

  // TODO: Update the following from PerAppTimeLimit policy.
  limits_reset_time_ = base::TimeDelta::FromHours(6);

  // TODO: Update the following from user pref.instead of setting it to Now().
  SetLastResetTime(base::Time::Now());
  ScheduleForTimeLimitReset();
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
  const base::Value* policy =
      pref_registrar_->prefs()->GetDictionary(prefs::kPerAppTimeLimitsPolicy);

  if (!policy || !policy->is_dict()) {
    LOG(WARNING) << "Invalid PerAppTimeLimits policy.";
    return;
  }

  app_registry_->UpdateAppLimits(policy::AppLimitsFromDict(*policy));

  base::Optional<base::TimeDelta> new_reset_time =
      policy::ResetTimeFromDict(*policy);
  // TODO(agawronska): Propagate the information about reset time change.
  if (new_reset_time && *new_reset_time != limits_reset_time_)
    limits_reset_time_ = *new_reset_time;
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

void AppTimeController::ShowAppTimeLimitNotification(
    const AppId& app_id,
    AppNotification notification) {
  // TODO(1015658): Show the notification.
  NOTIMPLEMENTED_LOG_ONCE();
}

base::Time AppTimeController::GetNextResetTime() const {
  // UTC time now.
  base::Time now = base::Time::Now();

  // UTC time local midnight.
  base::Time nearest_midnight = now.LocalMidnight();

  base::Time prev_midnight;
  if (now > nearest_midnight)
    prev_midnight = nearest_midnight;
  else
    prev_midnight = nearest_midnight - base::TimeDelta::FromHours(24);

  base::Time next_reset_time = prev_midnight + limits_reset_time_;

  if (next_reset_time > now)
    return next_reset_time;

  // We have already reset for this day. The reset time is the next day.
  return next_reset_time + base::TimeDelta::FromHours(24);
}

void AppTimeController::ScheduleForTimeLimitReset() {
  if (reset_timer_.IsRunning())
    reset_timer_.AbandonAndStop();

  base::TimeDelta time_until_reset = GetNextResetTime() - base::Time::Now();
  reset_timer_.Start(FROM_HERE, time_until_reset,
                     base::BindOnce(&AppTimeController::OnResetTimeReached,
                                    base::Unretained(this)));
}

void AppTimeController::OnResetTimeReached() {
  base::Time now = base::Time::Now();

  app_registry_->OnResetTimeReached(now);

  SetLastResetTime(now);

  ScheduleForTimeLimitReset();
}

void AppTimeController::SetLastResetTime(base::Time timestamp) {
  last_limits_reset_time_ = timestamp;
  // TODO(crbug.com/1015658) : |last_limits_reset_time_| should be persisted
  // across sessions.
}

}  // namespace app_time
}  // namespace chromeos
