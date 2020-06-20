// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/hermes/fake_hermes_manager_client.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "chromeos/dbus/hermes/hermes_profile_client.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {
const char* kDefaultMccMnc = "310999";
const char* kFakeActivationCodePrefix = "1$SMDP.GSMA.COM$00000-00000-00000-000";
const char* kFakeProfilePathPrefix = "/org/chromium/Hermes/Profile/";
const char* kFakeIccidPrefix = "10000000000000000";
const char* kFakeProfileNamePrefix = "FakeCellularNetwork_";
const char* kFakeServiceProvider = "Fake Wireless";
const char* kFakeNetworkServicePathPrefix = "/service/cellular1";

// Delay for slow methods or methods that involve network round-trips.
const base::TimeDelta interactive_delay = base::TimeDelta::FromSeconds(3);
}  // namespace

FakeHermesManagerClient::FakeHermesManagerClient() {
  ParseCommandLineSwitch();
}

FakeHermesManagerClient::~FakeHermesManagerClient() = default;

void FakeHermesManagerClient::AddCarrierProfile(
    const dbus::ObjectPath& path,
    const std::string& iccid,
    const std::string& name,
    const std::string& service_provider,
    const std::string& activation_code,
    const std::string& network_service_path,
    hermes::profile::State state) {
  DVLOG(1) << "Adding new profile path=" << path.value() << ", name=" << name
           << ", state=" << state;
  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(path);
  properties->iccid().ReplaceValue(iccid);
  properties->service_provider().ReplaceValue(service_provider);
  properties->mcc_mnc().ReplaceValue(kDefaultMccMnc);
  properties->activation_code().ReplaceValue(activation_code);
  properties->name().ReplaceValue(name);
  properties->nick_name().ReplaceValue(name);
  properties->state().ReplaceValue(state);
  profile_service_path_map_[path] = network_service_path;

  if (state == hermes::profile::State::kPending) {
    pending_profiles_.push_back(path);
    CallNotifyPendingCarrierProfileListChanged();
    return;
  }

  CreateCellularService(path);
  installed_profiles_.push_back(path);
  CallNotifyInstalledCarrierProfileListChanged();
}

void FakeHermesManagerClient::InstallProfileFromActivationCode(
    const std::string& activation_code,
    const std::string& confirmation_code,
    InstallCarrierProfileCallback callback) {
  DVLOG(1) << "Installing profile from activation code: code="
           << activation_code << ", confirmation_code=" << confirmation_code;
  if (!base::StartsWith(activation_code, kFakeActivationCodePrefix,
                        base::CompareCase::SENSITIVE)) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       HermesResponseStatus::kErrorInvalidActivationCode,
                       nullptr),
        interactive_delay);
    return;
  }

  dbus::ObjectPath profile_path =
      PopPendingProfileWithActivationCode(activation_code);
  if (profile_path.IsValid()) {
    // Move pending profile to installed.
    HermesProfileClient::Properties* properties =
        HermesProfileClient::Get()->GetProperties(profile_path);
    properties->state().ReplaceValue(hermes::profile::State::kInactive);
    installed_profiles_.push_back(profile_path);
    CallNotifyInstalledCarrierProfileListChanged();
  } else {
    // Create a new installed profile with given activation code.
    profile_path = AddFakeCarrierProfile(hermes::profile::State::kInactive,
                                         activation_code);
  }
  CreateCellularService(profile_path);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), HermesResponseStatus::kSuccess,
                     &profile_path),
      interactive_delay);
}

void FakeHermesManagerClient::InstallPendingProfile(
    const dbus::ObjectPath& carrier_profile_path,
    const std::string& confirmation_code,
    InstallCarrierProfileCallback callback) {
  DVLOG(1) << "Installing pending profile: path="
           << carrier_profile_path.value()
           << ", confirmation_code=" << confirmation_code;
  if (!PopPendingProfile(carrier_profile_path)) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), HermesResponseStatus::kErrorUnknown,
                       nullptr),
        interactive_delay);
    return;
  }

  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(carrier_profile_path);
  properties->state().ReplaceValue(hermes::profile::State::kInactive);
  installed_profiles_.push_back(carrier_profile_path);
  CallNotifyInstalledCarrierProfileListChanged();
  CreateCellularService(carrier_profile_path);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), HermesResponseStatus::kSuccess,
                     &carrier_profile_path),
      interactive_delay);
}

void FakeHermesManagerClient::RequestPendingEvents(
    HermesResponseCallback callback) {
  DVLOG(1) << "Pending Events Requested";
  if (!pending_event_requested_) {
    AddFakeCarrierProfile(hermes::profile::State::kPending, "");
    pending_event_requested_ = true;
  }
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), HermesResponseStatus::kSuccess),
      interactive_delay);
}

void FakeHermesManagerClient::UninstallProfile(
    const dbus::ObjectPath& carrier_profile_path,
    HermesResponseCallback callback) {
  std::vector<dbus::ObjectPath>::iterator it =
      std::find(installed_profiles_.begin(), installed_profiles_.end(),
                carrier_profile_path);
  if (it == installed_profiles_.end()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       HermesResponseStatus::kErrorUnknown),
        interactive_delay);
    return;
  }

  RemoveCellularService(carrier_profile_path);
  installed_profiles_.erase(it);
  CallNotifyInstalledCarrierProfileListChanged();
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), HermesResponseStatus::kSuccess),
      interactive_delay);
}

const std::vector<dbus::ObjectPath>&
FakeHermesManagerClient::GetInstalledCarrierProfiles() {
  return installed_profiles_;
}

const std::vector<dbus::ObjectPath>&
FakeHermesManagerClient::GetPendingCarrierProfiles() {
  return pending_profiles_;
}

HermesManagerClient::TestInterface*
FakeHermesManagerClient::GetTestInterface() {
  return this;
}

dbus::ObjectPath FakeHermesManagerClient::AddFakeCarrierProfile(
    hermes::profile::State state,
    std::string activation_code) {
  int index = fake_profile_counter_++;
  dbus::ObjectPath carrier_profile_path(
      base::StringPrintf("%s%02d", kFakeProfilePathPrefix, index));

  if (activation_code.empty()) {
    activation_code =
        base::StringPrintf("%s%02d", kFakeActivationCodePrefix, index);
  }
  AddCarrierProfile(
      carrier_profile_path,
      base::StringPrintf("%s%02d", kFakeIccidPrefix, index),
      base::StringPrintf("%s%02d", kFakeProfileNamePrefix, index),
      kFakeServiceProvider, activation_code,
      base::StringPrintf("%s%02d", kFakeNetworkServicePathPrefix, index),
      state);
  return carrier_profile_path;
}

void FakeHermesManagerClient::ParseCommandLineSwitch() {
  // Parse hermes stub commandline switch. Stubs are setup only if a value
  // of "on" is passed.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kHermesFake))
    return;
  std::string switch_value =
      command_line->GetSwitchValueASCII(switches::kHermesFake);
  if (switch_value != "on")
    return;

  // Add an installed stub carrier profile as initial environment.
  AddFakeCarrierProfile(hermes::profile::State::kInactive, "");
}

bool FakeHermesManagerClient::PopPendingProfile(
    dbus::ObjectPath carrier_profile_path) {
  std::vector<dbus::ObjectPath>::iterator it = std::find(
      pending_profiles_.begin(), pending_profiles_.end(), carrier_profile_path);
  if (it == pending_profiles_.end()) {
    return false;
  }

  pending_profiles_.erase(it);
  CallNotifyPendingCarrierProfileListChanged();
  return true;
}

dbus::ObjectPath FakeHermesManagerClient::PopPendingProfileWithActivationCode(
    const std::string& activation_code) {
  for (std::vector<dbus::ObjectPath>::iterator it = pending_profiles_.begin();
       it != pending_profiles_.end(); it++) {
    dbus::ObjectPath carrier_profile_path = *it;
    HermesProfileClient::Properties* properties =
        HermesProfileClient::Get()->GetProperties(carrier_profile_path);
    if (properties->activation_code().value() == activation_code) {
      pending_profiles_.erase(it);
      CallNotifyPendingCarrierProfileListChanged();
      return carrier_profile_path;
    }
  }
  return dbus::ObjectPath();
}

// Creates cellular service in shill for the given carrier profile path.
// This simulates the expected hermes - shill interaction when a new carrier
// profile is installed on the device through Hermes. Shill will be notified and
// it then creates cellular services with matching ICCID for this profile.
void FakeHermesManagerClient::CreateCellularService(
    const dbus::ObjectPath& carrier_profile_path) {
  const std::string& service_path =
      profile_service_path_map_[carrier_profile_path];
  HermesProfileClient::Properties* properties =
      HermesProfileClient::Get()->GetProperties(carrier_profile_path);
  ShillServiceClient::TestInterface* service_test =
      ShillServiceClient::Get()->GetTestInterface();
  service_test->AddService(service_path,
                           "esim_guid" + properties->iccid().value(),
                           properties->name().value(), shill::kTypeCellular,
                           shill::kStateIdle, true);
  service_test->SetServiceProperty(service_path, shill::kIccidProperty,
                                   base::Value(properties->iccid().value()));
  service_test->SetServiceProperty(
      service_path, shill::kImsiProperty,
      base::Value(properties->iccid().value() + "-IMSI"));
  service_test->SetServiceProperty(
      service_path, shill::kActivationStateProperty,
      base::Value(shill::kActivationStateActivated));
  service_test->SetServiceProperty(service_path, shill::kConnectableProperty,
                                   base::Value(false));
  service_test->SetServiceProperty(service_path, shill::kVisibleProperty,
                                   base::Value(true));

  ShillProfileClient::TestInterface* profile_test =
      ShillProfileClient::Get()->GetTestInterface();
  profile_test->AddService(ShillProfileClient::GetSharedProfilePath(),
                           service_path);
}

void FakeHermesManagerClient::RemoveCellularService(
    const dbus::ObjectPath& carrier_profile_path) {
  ShillServiceClient::TestInterface* service_test =
      ShillServiceClient::Get()->GetTestInterface();
  service_test->RemoveService(profile_service_path_map_[carrier_profile_path]);
  profile_service_path_map_.erase(carrier_profile_path);
}

void FakeHermesManagerClient::CallNotifyInstalledCarrierProfileListChanged() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeHermesManagerClient::NotifyInstalledCarrierProfileListChanged,
          base::Unretained(this)));
}

void FakeHermesManagerClient::CallNotifyPendingCarrierProfileListChanged() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &FakeHermesManagerClient::NotifyPendingCarrierProfileListChanged,
          base::Unretained(this)));
}

void FakeHermesManagerClient::NotifyInstalledCarrierProfileListChanged() {
  for (auto& observer : observers()) {
    observer.OnInstalledCarrierProfileListChanged();
  }
}

void FakeHermesManagerClient::NotifyPendingCarrierProfileListChanged() {
  for (auto& observer : observers()) {
    observer.OnPendingCarrierProfileListChanged();
  }
}

}  // namespace chromeos
