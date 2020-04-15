// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_METADATA_STORE_H_
#define CHROMEOS_NETWORK_NETWORK_METADATA_STORE_H_

#include <string>

#include "base/component_export.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "chromeos/network/network_configuration_observer.h"
#include "chromeos/network/network_connection_observer.h"
#include "chromeos/network/network_metadata_observer.h"

class PrefService;
class PrefRegistrySimple;

namespace base {
class TimeDelta;
}

namespace chromeos {

class NetworkConfigurationHandler;
class NetworkConnectionHandler;
class NetworkStateHandler;

// Stores metadata about networks using the UserProfilePrefStore for networks
// that are on the local profile and the LocalStatePrefStore for shared
// networks.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) NetworkMetadataStore
    : public NetworkConnectionObserver,
      public NetworkConfigurationObserver {
 public:
  NetworkMetadataStore(
      NetworkConfigurationHandler* network_configuration_handler,
      NetworkConnectionHandler* network_connection_handler,
      NetworkStateHandler* network_state_handler,
      PrefService* profile_pref_service,
      PrefService* device_pref_service);

  ~NetworkMetadataStore() override;

  // Registers preferences used by this class in the provided |registry|.  This
  // should be called for both the Profile registry and the Local State registry
  // prior to using this class.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  // NetworkConnectionObserver::
  void ConnectSucceeded(const std::string& service_path) override;

  // NetworkConfigurationObserver::
  void OnConfigurationCreated(const std::string& service_path,
                              const std::string& guid) override;
  void OnConfigurationModified(const std::string& service_path,
                               const std::string& guid,
                               base::DictionaryValue* set_properties) override;
  void OnConfigurationRemoved(const std::string& service_path,
                              const std::string& guid) override;

  // Records that the network was added by sync.
  void SetIsConfiguredBySync(const std::string& network_guid);

  // Returns the timestamp when the network was last connected to, or 0 if it
  // has never had a successful connection.
  base::TimeDelta GetLastConnectedTimestamp(const std::string& network_guid);
  void SetLastConnectedTimestamp(const std::string& network_guid,
                                 const base::TimeDelta& timestamp);

  // Networks which were added directly from sync data will return true.
  bool GetIsConfiguredBySync(const std::string& network_guid);

  // Networks which were created by the logged in user will return true.
  bool GetIsCreatedByUser(const std::string& network_guid);

  // Manage observers.
  void AddObserver(NetworkMetadataObserver* observer);
  void RemoveObserver(NetworkMetadataObserver* observer);

  base::WeakPtr<NetworkMetadataStore> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void RemoveNetworkFromPref(const std::string& network_guid,
                             PrefService* pref_service);
  void SetPref(const std::string& network_guid,
               const std::string& key,
               base::Value value);
  const base::Value* GetPref(const std::string& network_guid,
                             const std::string& key);

  base::ObserverList<NetworkMetadataObserver> observers_;
  NetworkConfigurationHandler* network_configuration_handler_;
  NetworkConnectionHandler* network_connection_handler_;
  NetworkStateHandler* network_state_handler_;
  PrefService* profile_pref_service_;
  PrefService* device_pref_service_;
  base::WeakPtrFactory<NetworkMetadataStore> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_METADATA_STORE_H_
