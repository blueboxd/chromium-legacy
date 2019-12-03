// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/calculators_policies_binder.h"

#include <list>
#include <map>
#include <string>
#include <vector>

#include "chrome/browser/chromeos/printing/bulk_printers_calculator.h"
#include "chrome/browser/chromeos/printing/bulk_printers_calculator_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

namespace chromeos {

namespace {

BulkPrintersCalculator::AccessMode ConvertToAccessMode(int mode_val) {
  if (mode_val >= BulkPrintersCalculator::BLACKLIST_ONLY &&
      mode_val <= BulkPrintersCalculator::ALL_ACCESS) {
    return static_cast<BulkPrintersCalculator::AccessMode>(mode_val);
  }
  // Error occurred, let's return the default value.
  LOG(ERROR) << "Unrecognized access mode";
  return BulkPrintersCalculator::ALL_ACCESS;
}

std::vector<std::string> ConvertToVector(const base::ListValue* list) {
  std::vector<std::string> string_list;
  if (!list) {
    return string_list;
  }

  for (const base::Value& value : *list) {
    if (value.is_string()) {
      string_list.push_back(value.GetString());
    }
  }
  return string_list;
}

class PrefBinder : public CalculatorsPoliciesBinder {
 public:
  PrefBinder(PrefService* pref_service,
             base::WeakPtr<BulkPrintersCalculator> calculator)
      : CalculatorsPoliciesBinder(prefs::kRecommendedNativePrintersAccessMode,
                                  prefs::kRecommendedNativePrintersBlacklist,
                                  prefs::kRecommendedNativePrintersWhitelist,
                                  calculator),
        prefs_(pref_service) {
    pref_change_registrar_.Init(prefs_);
  }

 protected:
  void Bind(const char* policy_name, base::RepeatingClosure closure) override {
    DVLOG(1) << "Binding " << policy_name;
    pref_change_registrar_.Add(policy_name, closure);
  }

  int GetAccessMode(const char* name) const override {
    return prefs_->GetInteger(name);
  }

  std::vector<std::string> GetStringList(const char* name) const override {
    return ConvertToVector(prefs_->GetList(name));
  }

 private:
  PrefService* prefs_;
  PrefChangeRegistrar pref_change_registrar_;
};

class SettingsBinder : public CalculatorsPoliciesBinder {
 public:
  SettingsBinder(CrosSettings* settings,
                 base::WeakPtr<BulkPrintersCalculator> calculator)
      : CalculatorsPoliciesBinder(kDeviceNativePrintersAccessMode,
                                  kDeviceNativePrintersBlacklist,
                                  kDeviceNativePrintersWhitelist,
                                  calculator),
        settings_(settings) {}

 protected:
  void Bind(const char* policy_name, base::RepeatingClosure closure) override {
    DVLOG(1) << "Bind device setting: " << policy_name;
    subscriptions_.push_back(
        settings_->AddSettingsObserver(policy_name, closure));
  }

  int GetAccessMode(const char* name) const override {
    int mode_val;
    if (!settings_->GetInteger(kDeviceNativePrintersAccessMode, &mode_val)) {
      mode_val = BulkPrintersCalculator::AccessMode::UNSET;
    }
    DVLOG(1) << "Device access mode: " << mode_val;
    return mode_val;
  }

  std::vector<std::string> GetStringList(const char* name) const override {
    const base::ListValue* list;
    if (!settings_->GetList(name, &list)) {
      list = nullptr;
    }
    return ConvertToVector(list);
  }

 private:
  CrosSettings* settings_;
  std::list<std::unique_ptr<CrosSettings::ObserverSubscription>> subscriptions_;
};

}  // namespace

// static
void CalculatorsPoliciesBinder::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  // Default value for access mode is AllAccess.
  registry->RegisterIntegerPref(prefs::kRecommendedNativePrintersAccessMode,
                                BulkPrintersCalculator::ALL_ACCESS);
  registry->RegisterListPref(prefs::kRecommendedNativePrintersBlacklist);
  registry->RegisterListPref(prefs::kRecommendedNativePrintersWhitelist);
}

// static
std::unique_ptr<CalculatorsPoliciesBinder>
CalculatorsPoliciesBinder::DeviceBinder(
    CrosSettings* settings,
    base::WeakPtr<BulkPrintersCalculator> calculator) {
  auto binder = std::make_unique<SettingsBinder>(settings, calculator);
  binder->Init();
  return binder;
}

// static
std::unique_ptr<CalculatorsPoliciesBinder>
CalculatorsPoliciesBinder::UserBinder(
    PrefService* prefs,
    base::WeakPtr<BulkPrintersCalculator> calculator) {
  auto binder = std::make_unique<PrefBinder>(prefs, calculator);
  binder->Init();
  return binder;
}

CalculatorsPoliciesBinder::CalculatorsPoliciesBinder(
    const char* access_mode_name,
    const char* blacklist_name,
    const char* whitelist_name,
    base::WeakPtr<BulkPrintersCalculator> calculator)
    : access_mode_name_(access_mode_name),
      blacklist_name_(blacklist_name),
      whitelist_name_(whitelist_name),
      calculator_(calculator) {
  DCHECK(access_mode_name);
  DCHECK(blacklist_name);
  DCHECK(whitelist_name);
  DCHECK(calculator);
}

CalculatorsPoliciesBinder::~CalculatorsPoliciesBinder() = default;

base::WeakPtr<CalculatorsPoliciesBinder>
CalculatorsPoliciesBinder::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void CalculatorsPoliciesBinder::Init() {
  // Register for future updates.
  Bind(access_mode_name_,
       base::BindRepeating(&CalculatorsPoliciesBinder::UpdateAccessMode,
                           GetWeakPtr()));
  Bind(blacklist_name_,
       base::BindRepeating(&CalculatorsPoliciesBinder::UpdateBlacklist,
                           GetWeakPtr()));
  Bind(whitelist_name_,
       base::BindRepeating(&CalculatorsPoliciesBinder::UpdateWhitelist,
                           GetWeakPtr()));

  // Retrieve initial values for all policy fields.
  UpdateAccessMode();
  UpdateBlacklist();
  UpdateWhitelist();
}

void CalculatorsPoliciesBinder::UpdateAccessMode() {
  DVLOG(1) << "Update access mode";
  if (calculator_) {
    calculator_->SetAccessMode(
        ConvertToAccessMode(GetAccessMode(access_mode_name_)));
  }
}

void CalculatorsPoliciesBinder::UpdateWhitelist() {
  if (calculator_) {
    calculator_->SetWhitelist(GetStringList(whitelist_name_));
  }
}

void CalculatorsPoliciesBinder::UpdateBlacklist() {
  if (calculator_) {
    calculator_->SetBlacklist(GetStringList(blacklist_name_));
  }
}

}  // namespace chromeos
