// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_HERMES_HERMES_MANAGER_CLIENT_H_
#define CHROMEOS_DBUS_HERMES_HERMES_MANAGER_CLIENT_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/hermes/hermes_response_status.h"
#include "dbus/property.h"
#include "third_party/cros_system_api/dbus/hermes/dbus-constants.h"

namespace dbus {
class Bus;
class ObjectPath;
}  // namespace dbus

namespace chromeos {

// HermesManagerClient is used to talk to the main Hermes Manager dbus object.
class COMPONENT_EXPORT(HERMES_CLIENT) HermesManagerClient {
 public:
  // Callback for profile installation methods. Callback returns status code
  // and the object path for the profile that was just successfully installed.
  using InstallCarrierProfileCallback =
      base::OnceCallback<void(HermesResponseStatus status,
                              const dbus::ObjectPath* carrier_profile_path)>;

  // Interface for setting up and manipulating profiles in a testing
  // environment.
  class TestInterface {
   public:
    // Adds a new carrier profile with given path and properties.
    virtual void AddCarrierProfile(const dbus::ObjectPath& path,
                                   const std::string& iccid,
                                   const std::string& name,
                                   const std::string& service_provider,
                                   const std::string& activation_code,
                                   const std::string& network_service_path,
                                   hermes::profile::State state) = 0;
  };

  // Interface for observing Hermes Manager changes.
  class Observer {
   public:
    virtual ~Observer() = default;

    // Called when new profiles are installed or removed.
    virtual void OnInstalledCarrierProfileListChanged() {}

    // Called when new pending profile list is updated.
    virtual void OnPendingCarrierProfileListChanged() {}
  };

  // Adds an observer for carrier profile lists changes on Hermes manager.
  virtual void AddObserver(Observer* observer);

  // Removes an observer for Hermes manager.
  virtual void RemoveObserver(Observer* observer);

  // Install a carrier profile given the |activation_code| and
  // |conirmation_code|. |confirmation_code| can be empty if no confirmation is
  // required by carrier. Returns the object path to the carrier profile that
  // was just installed.
  virtual void InstallProfileFromActivationCode(
      const std::string& activation_code,
      const std::string& confirmation_code,
      InstallCarrierProfileCallback callback) = 0;

  // Installs a pending profile with given |carrier_profile_path|.
  // |confirmation_code| can be empty if no confirmation is required by carrier.
  // Returns the object path to the carrier profile that was just installed.
  virtual void InstallPendingProfile(
      const dbus::ObjectPath& carrier_profile_path,
      const std::string& confirmation_code,
      InstallCarrierProfileCallback callback) = 0;

  // Updates pending profiles for the device from the SMDS server. This updates
  // pending profiles list prior to returning.
  virtual void RequestPendingEvents(HermesResponseCallback callback) = 0;

  // Removes the carrier profile with the given |carrier_profile_path| from
  // the device. Returns a response status indicating the result of the
  // operation.
  virtual void UninstallProfile(const dbus::ObjectPath& carrier_profile_path,
                                HermesResponseCallback callback) = 0;

  // Returns the list of all installed carrier profiles.
  virtual const std::vector<dbus::ObjectPath>&
  GetInstalledCarrierProfiles() = 0;

  // Returns the list of carrier profiles that are available for installation.
  virtual const std::vector<dbus::ObjectPath>& GetPendingCarrierProfiles() = 0;

  // Returns an instance of Hermes Manager Test interface.
  virtual TestInterface* GetTestInterface() = 0;

  // Creates and initializes the global instance.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a global fake instance.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance.
  static HermesManagerClient* Get();

 protected:
  HermesManagerClient();
  virtual ~HermesManagerClient();

  const base::ObserverList<HermesManagerClient::Observer>::Unchecked&
  observers() {
    return observers_;
  }

 private:
  base::ObserverList<HermesManagerClient::Observer>::Unchecked observers_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_HERMES_HERMES_MANAGER_CLIENT_H_
