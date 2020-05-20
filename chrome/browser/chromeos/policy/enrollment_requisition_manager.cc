// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/enrollment_requisition_manager.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/common/pref_names.h"
#include "chromeos/system/statistics_provider.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace policy {

namespace {

// Well-known requisition types.
const char kNoRequisition[] = "none";
const char kRemoraRequisition[] = "remora";
const char kSharkRequisition[] = "shark";
const char kRialtoRequisition[] = "rialto";

// Fetches a machine statistic value from StatisticsProvider, returns an empty
// string on failure.
std::string GetMachineStatistic(const std::string& key) {
  std::string value;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineStatistic(key, &value))
    return std::string();

  return value;
}

// Gets a machine flag from StatisticsProvider, returns the given
// |default_value| if not present.
bool GetMachineFlag(const std::string& key, bool default_value) {
  bool value = default_value;
  chromeos::system::StatisticsProvider* provider =
      chromeos::system::StatisticsProvider::GetInstance();
  if (!provider->GetMachineFlag(key, &value))
    return default_value;

  return value;
}

}  // namespace

EnrollmentRequisitionManager::EnrollmentRequisitionManager()
    : local_state_(nullptr) {}

void EnrollmentRequisitionManager::Initialize(PrefService* local_state) {
  CHECK(local_state);

  local_state_ = local_state;

  InitializeRequisition();
}

std::string EnrollmentRequisitionManager::GetDeviceRequisition() const {
  std::string requisition;
  const PrefService::Preference* pref =
      local_state_->FindPreference(prefs::kDeviceEnrollmentRequisition);
  if (!pref->IsDefaultValue())
    pref->GetValue()->GetAsString(&requisition);

  if (requisition == kNoRequisition)
    requisition.clear();

  return requisition;
}

void EnrollmentRequisitionManager::SetDeviceRequisition(
    const std::string& requisition) {
  VLOG(1) << "SetDeviceRequisition " << requisition;
  if (!local_state_)
    return;
  if (requisition.empty()) {
    local_state_->ClearPref(prefs::kDeviceEnrollmentRequisition);
    local_state_->ClearPref(prefs::kDeviceEnrollmentAutoStart);
    local_state_->ClearPref(prefs::kDeviceEnrollmentCanExit);
  } else {
    local_state_->SetString(prefs::kDeviceEnrollmentRequisition, requisition);
    if (requisition == kNoRequisition) {
      local_state_->ClearPref(prefs::kDeviceEnrollmentAutoStart);
      local_state_->ClearPref(prefs::kDeviceEnrollmentCanExit);
    } else {
      SetDeviceEnrollmentAutoStart();
    }
  }
}

bool EnrollmentRequisitionManager::IsRemoraRequisition() const {
  return GetDeviceRequisition() == kRemoraRequisition;
}

bool EnrollmentRequisitionManager::IsSharkRequisition() const {
  return GetDeviceRequisition() == kSharkRequisition;
}

std::string EnrollmentRequisitionManager::GetSubOrganization() const {
  if (!local_state_)
    return std::string();
  std::string sub_organization;
  const PrefService::Preference* pref =
      local_state_->FindPreference(prefs::kDeviceEnrollmentSubOrganization);
  if (!pref->IsDefaultValue())
    pref->GetValue()->GetAsString(&sub_organization);
  return sub_organization;
}

void EnrollmentRequisitionManager::SetSubOrganization(
    const std::string& sub_organization) {
  if (!local_state_)
    return;
  if (sub_organization.empty())
    local_state_->ClearPref(prefs::kDeviceEnrollmentSubOrganization);
  else
    local_state_->SetString(prefs::kDeviceEnrollmentSubOrganization,
                            sub_organization);
}

void EnrollmentRequisitionManager::SetDeviceEnrollmentAutoStart() {
  if (local_state_) {
    local_state_->SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
    local_state_->SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
  }
}

// static
void EnrollmentRequisitionManager::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kDeviceEnrollmentRequisition,
                               std::string());
  registry->RegisterStringPref(prefs::kDeviceEnrollmentSubOrganization,
                               std::string());
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentAutoStart, false);
  registry->RegisterBooleanPref(prefs::kDeviceEnrollmentCanExit, true);
}

void EnrollmentRequisitionManager::InitializeRequisition() {
  // OEM statistics are only loaded when OOBE is not completed.
  if (chromeos::StartupUtils::IsOobeCompleted())
    return;

  // Demo requisition may have been set in a prior enrollment attempt that was
  // interrupted.
  chromeos::DemoSetupController::ClearDemoRequisition(this);
  const PrefService::Preference* pref =
      local_state_->FindPreference(prefs::kDeviceEnrollmentRequisition);
  if (pref->IsDefaultValue()) {
    std::string requisition =
        GetMachineStatistic(chromeos::system::kOemDeviceRequisitionKey);

    if (!requisition.empty()) {
      local_state_->SetString(prefs::kDeviceEnrollmentRequisition, requisition);
      if (requisition == kRemoraRequisition ||
          requisition == kSharkRequisition ||
          requisition == kRialtoRequisition) {
        SetDeviceEnrollmentAutoStart();
      } else {
        local_state_->SetBoolean(
            prefs::kDeviceEnrollmentAutoStart,
            GetMachineFlag(chromeos::system::kOemIsEnterpriseManagedKey,
                           false));
        local_state_->SetBoolean(
            prefs::kDeviceEnrollmentCanExit,
            GetMachineFlag(chromeos::system::kOemCanExitEnterpriseEnrollmentKey,
                           false));
      }
    }
  }
}

}  // namespace policy
