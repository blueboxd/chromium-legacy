// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/time/default_clock.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler_delegate_impl.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/cros_settings_provider.h"

using MinimumVersionRequirement =
    policy::MinimumVersionPolicyHandler::MinimumVersionRequirement;

namespace policy {

const char MinimumVersionPolicyHandler::kChromeVersion[] = "chrome_version";
const char MinimumVersionPolicyHandler::kWarningPeriod[] = "warning_period";
const char MinimumVersionPolicyHandler::KEolWarningPeriod[] =
    "eol_warning_period";

MinimumVersionRequirement::MinimumVersionRequirement(
    const base::Version version,
    const base::TimeDelta warning,
    const base::TimeDelta eol_warning)
    : minimum_version_(version),
      warning_time_(warning),
      eol_warning_time_(eol_warning) {}

std::unique_ptr<MinimumVersionRequirement>
MinimumVersionRequirement::CreateInstanceIfValid(
    const base::DictionaryValue* dict) {
  const std::string* version = dict->FindStringPath(kChromeVersion);
  if (!version)
    return nullptr;
  base::Version minimum_version(*version);
  if (!minimum_version.IsValid())
    return nullptr;
  auto warning = dict->FindIntPath(kWarningPeriod);
  base::TimeDelta warning_time =
      base::TimeDelta::FromDays(warning.has_value() ? warning.value() : 0);
  auto eol_warning = dict->FindIntPath(KEolWarningPeriod);
  base::TimeDelta eol_warning_time = base::TimeDelta::FromDays(
      eol_warning.has_value() ? eol_warning.value() : 0);
  return std::make_unique<MinimumVersionRequirement>(
      minimum_version, warning_time, eol_warning_time);
}

int MinimumVersionRequirement::Compare(
    const MinimumVersionRequirement* other) const {
  const int version_compare = version().CompareTo(other->version());
  if (version_compare != 0)
    return version_compare;
  if (warning() != other->warning())
    return (warning() > other->warning() ? 1 : -1);
  if (eol_warning() != other->eol_warning())
    return (eol_warning() > other->eol_warning() ? 1 : -1);
  return 0;
}

MinimumVersionPolicyHandler::MinimumVersionPolicyHandler(
    Delegate* delegate,
    chromeos::CrosSettings* cros_settings)
    : delegate_(delegate),
      cros_settings_(cros_settings),
      clock_(base::DefaultClock::GetInstance()) {
  policy_subscription_ = cros_settings_->AddSettingsObserver(
      chromeos::kMinimumChromeVersionEnforced,
      base::Bind(&MinimumVersionPolicyHandler::OnPolicyChanged,
                 weak_factory_.GetWeakPtr()));
  // Fire it once so we're sure we get an invocation on startup.
  OnPolicyChanged();
}

MinimumVersionPolicyHandler::~MinimumVersionPolicyHandler() = default;

void MinimumVersionPolicyHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MinimumVersionPolicyHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool MinimumVersionPolicyHandler::CurrentVersionSatisfies(
    const MinimumVersionRequirement& requirement) const {
  return delegate_->GetCurrentVersion().CompareTo(requirement.version()) >= 0;
}

bool MinimumVersionPolicyHandler::IsPolicyApplicable() {
  bool device_managed = delegate_->IsEnterpriseManaged();
  bool is_kiosk = delegate_->IsKioskMode();
  return device_managed && !is_kiosk;
}

void MinimumVersionPolicyHandler::OnPolicyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::BindOnce(&MinimumVersionPolicyHandler::OnPolicyChanged,
                         weak_factory_.GetWeakPtr()));
  if (status != chromeos::CrosSettingsProvider::TRUSTED ||
      !IsPolicyApplicable())
    return;

  const base::ListValue* entries;
  std::vector<std::unique_ptr<MinimumVersionRequirement>> configs;
  if (!cros_settings_->GetList(chromeos::kMinimumChromeVersionEnforced,
                               &entries) ||
      !entries->GetSize()) {
    // Reset state and hide update required screen if policy is not set or set
    // to empty list.
    HandleUpdateNotRequired();
    return;
  }

  for (const auto& item : entries->GetList()) {
    const base::DictionaryValue* dict;
    if (item.GetAsDictionary(&dict)) {
      std::unique_ptr<MinimumVersionRequirement> instance =
          MinimumVersionRequirement::CreateInstanceIfValid(dict);
      if (instance)
        configs.push_back(std::move(instance));
    }
  }

  // Select the strongest config whose requirements are not satisfied by the
  // current version. The strongest config is chosen as the one whose minimum
  // required version is greater and closest to the current version. In case of
  // conflict. preference is given to the one with lesser warning time or eol
  // warning time.
  int strongest_config_idx = -1;
  std::vector<std::unique_ptr<MinimumVersionRequirement>>
      update_required_configs;
  for (unsigned int i = 0; i < configs.size(); i++) {
    MinimumVersionRequirement* item = configs[i].get();
    if (!CurrentVersionSatisfies(*item) &&
        (strongest_config_idx == -1 ||
         item->Compare(configs[strongest_config_idx].get()) < 0))
      strongest_config_idx = i;
  }

  if (strongest_config_idx != -1) {
    // Update is required if at least one config exists whose requirements are
    // not satisfied by the current version.
    std::unique_ptr<MinimumVersionRequirement> strongest_config =
        std::move(configs[strongest_config_idx]);
    if (!state_ || state_->Compare(strongest_config.get()) != 0) {
      state_ = std::move(strongest_config);
      requirements_met_ = false;
      FetchEolInfo();
    }
  } else if (state_) {
    // Update is not required as the requirements of all of the configs in the
    // policy are satisfied by the current chrome version.
    HandleUpdateNotRequired();
  }
}

void MinimumVersionPolicyHandler::HandleUpdateNotRequired() {
  // Reset the state including any running timers.
  Reset();
  // Hide update required screen if it is visible and switch back to the login
  // screen.
  if (delegate_->IsLoginSessionState()) {
    delegate_->HideUpdateRequiredScreenIfShown();
  }
}

void MinimumVersionPolicyHandler::Reset() {
  state_.reset();
  requirements_met_ = true;
  deadline_reached = false;
}

void MinimumVersionPolicyHandler::FetchEolInfo() {
  // Return if update required state is null meaning all requirements are
  // satisfied.
  if (!state_)
    return;

  update_required_time_ = clock_->Now();
  chromeos::UpdateEngineClient* update_engine_client =
      chromeos::DBusThreadManager::Get()->GetUpdateEngineClient();
  // Request the End of Life (Auto Update Expiration) status.
  update_engine_client->GetEolInfo(
      base::BindOnce(&MinimumVersionPolicyHandler::OnFetchEolInfo,
                     weak_factory_.GetWeakPtr()));
}

void MinimumVersionPolicyHandler::OnFetchEolInfo(
    const chromeos::UpdateEngineClient::EolInfo info) {
  if (info.eol_date.is_null() || info.eol_date > update_required_time_) {
    // End of life is not reached. Start update with |warning_time_|.
    HandleUpdateRequired(state_->warning());
  } else {
    // End of life is reached. Start update with |eol_warning_time_|.
    HandleUpdateRequired(state_->eol_warning());
  }
}

void MinimumVersionPolicyHandler::HandleUpdateRequired(
    base::TimeDelta warning_time) {
  if (warning_time.is_zero())
    OnDeadlineReached();
  // TODO(https://crbug.com/1048607): Handle the cases for network limitations
  // and warning time greater than zero. This would include starting the timer,
  // triggering update process if network is good and showing in-session
  // notifications if required.
}

void MinimumVersionPolicyHandler::OnDeadlineReached() {
  deadline_reached = true;
  if (delegate_->IsLoginSessionState() && !delegate_->IsLoginInProgress()) {
    // Show update required screen over the login screen.
    delegate_->ShowUpdateRequiredScreen();
  } else if (delegate_->IsUserLoggedIn() && delegate_->IsUserManaged()) {
    // Terminate the current user session to show update required
    // screen on the login screen if user is managed.
    delegate_->RestartToLoginScreen();
  }
  // No action is required if -
  // 1) The user signed in is not managed. Once the un-managed user signs out or
  // the device is rebooted, the policy handler will be called again to show the
  // update required screen if required.
  // 2) Login is in progress. This would be handled in-session once user logs
  // in, the user would be logged out and update required screen is shown.
  // 3) Device has just been enrolled. The login screen would check and show the
  // update required screen.
}

}  // namespace policy
