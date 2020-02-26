// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/minimum_version_policy_handler.h"

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/minimum_version_policy_handler_delegate_impl.h"
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
    : delegate_(delegate), cros_settings_(cros_settings) {
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

void MinimumVersionPolicyHandler::NotifyMinimumVersionStateChanged() {
  for (auto& observer : observers_)
    observer.OnMinimumVersionStateChanged();
}

bool MinimumVersionPolicyHandler::IsPolicyApplicable() {
  bool device_managed = delegate_->IsEnterpriseManaged();
  bool is_kiosk = delegate_->IsKioskMode();
  return device_managed && !is_kiosk;
}

void MinimumVersionPolicyHandler::OnPolicyChanged() {
  chromeos::CrosSettingsProvider::TrustedStatus status =
      cros_settings_->PrepareTrustedValues(
          base::Bind(&MinimumVersionPolicyHandler::OnPolicyChanged,
                     weak_factory_.GetWeakPtr()));
  if (status != chromeos::CrosSettingsProvider::TRUSTED ||
      !IsPolicyApplicable())
    return;
  const base::ListValue* entries;
  std::vector<std::unique_ptr<MinimumVersionRequirement>> configs;
  if (!cros_settings_->GetList(chromeos::kMinimumChromeVersionEnforced,
                               &entries) ||
      !entries->GetSize()) {
    // Reset and notify if policy is not set or set to empty list.
    Reset();
    NotifyMinimumVersionStateChanged();
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
  // select the strongest config whose requirements are not satisfied by the
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
    // update is required if at least one config exists whose requirements are
    // not satisfied by the current version.
    std::unique_ptr<MinimumVersionRequirement> strongest_config =
        std::move(configs[strongest_config_idx]);
    if (!state_ || state_->Compare(strongest_config.get()) != 0) {
      state_ = std::move(strongest_config);
      requirements_met_ = false;
      NotifyMinimumVersionStateChanged();
    }
  } else if (state_) {
    // Reset the state if the policy is already applied.
    Reset();
    NotifyMinimumVersionStateChanged();
  }
}

void MinimumVersionPolicyHandler::Reset() {
  state_.reset();
  requirements_met_ = true;
}

}  // namespace policy
