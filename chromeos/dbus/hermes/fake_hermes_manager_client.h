// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HERMES_FAKE_HERMES_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_HERMES_FAKE_HERMES_MANAGER_CLIENT_H_

#include <set>

#include "base/component_export.h"
#include "base/macros.h"
#include "chromeos/dbus/hermes/hermes_manager_client.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace chromeos {

// Fake implementation for HermesManagerClient. This class interacts with
// FakeHermesDeviceClient and FakeShillManagerClient to setup stub carrier
// profile objects and corresponding network services.
class COMPONENT_EXPORT(HERMES_CLIENT) FakeHermesManagerClient
    : public HermesManagerClient,
      public HermesManagerClient::TestInterface {
 public:
  FakeHermesManagerClient();
  FakeHermesManagerClient(const FakeHermesManagerClient&) = delete;
  ~FakeHermesManagerClient() override;

  // HermesManagerClient::TestInterface:
  void AddCarrierProfile(const dbus::ObjectPath& path,
                         const std::string& iccid,
                         const std::string& name,
                         const std::string& service_provider,
                         const std::string& activation_code,
                         const std::string& network_service_path,
                         hermes::profile::State state) override;

  // HermesManagerClient:
  void InstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallCarrierProfileCallback callback) override;

  void InstallPendingProfile(const dbus::ObjectPath& carrier_profile_path,
                             const std::string& confirmation_code,
                             InstallCarrierProfileCallback callback) override;

  void RequestPendingEvents(HermesResponseCallback callback) override;

  void UninstallProfile(const dbus::ObjectPath& carrier_profile_path,
                        HermesResponseCallback callback) override;

  const std::vector<dbus::ObjectPath>& GetInstalledCarrierProfiles() override;

  const std::vector<dbus::ObjectPath>& GetPendingCarrierProfiles() override;

  HermesManagerClient::TestInterface* GetTestInterface() override;

  FakeHermesManagerClient& operator=(const FakeHermesManagerClient&) = delete;

 private:
  dbus::ObjectPath AddFakeCarrierProfile(hermes::profile::State state,
                                         std::string activation_code);
  void ParseCommandLineSwitch();
  bool PopPendingProfile(dbus::ObjectPath carrier_profile_path);
  dbus::ObjectPath PopPendingProfileWithActivationCode(
      const std::string& activation_code);
  void CreateCellularService(const dbus::ObjectPath& carrier_profile_path);
  void RemoveCellularService(const dbus::ObjectPath& carrier_profile_path);
  void CallNotifyInstalledCarrierProfileListChanged();
  void CallNotifyPendingCarrierProfileListChanged();
  void NotifyInstalledCarrierProfileListChanged();
  void NotifyPendingCarrierProfileListChanged();

  // Indicates whether a pending event request has already been made.
  bool pending_event_requested_ = false;

  int fake_profile_counter_ = 0;

  // Mapping between carrier profile objects and their corresponding
  // shill network service paths.
  std::map<dbus::ObjectPath, std::string> profile_service_path_map_;

  // List of paths to installed and pending profile objects.
  std::vector<dbus::ObjectPath> installed_profiles_;
  std::vector<dbus::ObjectPath> pending_profiles_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_HERMES_FAKE_HERMES_MANAGER_CLIENT_H_
