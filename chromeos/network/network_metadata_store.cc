// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_metadata_store.h"

#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {

namespace {

const char kNetworkMetadataPref[] = "network_metadata";
const char kLastConnectedTimestampPref[] = "last_connected_timestamp";
const char kIsFromSync[] = "is_from_sync";
const char kOwner[] = "owner";

std::string GetPath(const std::string& guid, const std::string& subkey) {
  return base::StringPrintf("%s.%s", guid.c_str(), subkey.c_str());
}

}  // namespace

// static
void NetworkMetadataStore::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(kNetworkMetadataPref);
}

NetworkMetadataStore::NetworkMetadataStore(
    NetworkConfigurationHandler* network_configuration_handler,
    NetworkConnectionHandler* network_connection_handler,
    NetworkStateHandler* network_state_handler,
    PrefService* profile_pref_service,
    PrefService* device_pref_service)
    : network_configuration_handler_(network_configuration_handler),
      network_connection_handler_(network_connection_handler),
      network_state_handler_(network_state_handler),
      profile_pref_service_(profile_pref_service),
      device_pref_service_(device_pref_service) {
  if (network_connection_handler_) {
    network_connection_handler_->AddObserver(this);
  }
  if (network_configuration_handler_) {
    network_configuration_handler_->AddObserver(this);
  }
}

NetworkMetadataStore::~NetworkMetadataStore() {
  if (network_connection_handler_) {
    network_connection_handler_->RemoveObserver(this);
  }
  if (network_configuration_handler_) {
    network_configuration_handler_->RemoveObserver(this);
  }
}

void NetworkMetadataStore::ConnectSucceeded(const std::string& service_path) {
  const NetworkState* network =
      network_state_handler_->GetNetworkState(service_path);

  if (!network || network->type() != shill::kTypeWifi) {
    return;
  }

  bool is_first_connection =
      GetLastConnectedTimestamp(network->guid()).is_zero();

  SetLastConnectedTimestamp(network->guid(),
                            base::Time::Now().ToDeltaSinceWindowsEpoch());

  if (is_first_connection) {
    for (auto& observer : observers_) {
      observer.OnFirstConnectionToNetwork(network->guid());
    }
  }
}

void NetworkMetadataStore::OnConfigurationCreated(
    const std::string& service_path,
    const std::string& guid) {
  if (!user_manager::UserManager::IsInitialized())
    return;

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user) {
    NET_LOG(EVENT)
        << "Network added with no active user, owner metadata not recorded.";
    return;
  }

  SetPref(guid, kOwner, base::Value(user->username_hash()));
}

void NetworkMetadataStore::OnConfigurationModified(
    const std::string& service_path,
    const std::string& guid,
    base::DictionaryValue* set_properties) {
  if (!set_properties) {
    return;
  }

  SetPref(guid, kIsFromSync, base::Value(false));

  // Only clear last connected if the passphrase changes.  Other settings
  // (autoconnect, dns, etc.) won't affect the ability to connect to a network.
  if (set_properties->HasKey(shill::kPassphraseProperty)) {
    SetPref(guid, kLastConnectedTimestampPref, base::Value(0));
  }

  for (auto& observer : observers_) {
    observer.OnNetworkUpdate(guid, set_properties);
  }
}

void NetworkMetadataStore::OnConfigurationRemoved(
    const std::string& service_path,
    const std::string& network_guid) {
  RemoveNetworkFromPref(network_guid, device_pref_service_);
  RemoveNetworkFromPref(network_guid, profile_pref_service_);
}

void NetworkMetadataStore::RemoveNetworkFromPref(
    const std::string& network_guid,
    PrefService* pref_service) {
  if (!pref_service) {
    return;
  }

  const base::DictionaryValue* dict =
      pref_service->GetDictionary(kNetworkMetadataPref);
  if (!dict || !dict->HasKey(network_guid)) {
    return;
  }

  base::Value writeable_dict = dict->Clone();
  if (!writeable_dict.RemoveKey(network_guid)) {
    return;
  }

  pref_service->Set(kNetworkMetadataPref, writeable_dict);
}

void NetworkMetadataStore::SetIsConfiguredBySync(
    const std::string& network_guid) {
  SetPref(network_guid, kIsFromSync, base::Value(true));
}

base::TimeDelta NetworkMetadataStore::GetLastConnectedTimestamp(
    const std::string& network_guid) {
  const base::Value* timestamp =
      GetPref(network_guid, kLastConnectedTimestampPref);

  if (!timestamp || !timestamp->is_double()) {
    return base::TimeDelta();
  }

  return base::TimeDelta::FromMillisecondsD(timestamp->GetDouble());
}

void NetworkMetadataStore::SetLastConnectedTimestamp(
    const std::string& network_guid,
    const base::TimeDelta& timestamp) {
  double timestamp_f = timestamp.InMillisecondsF();
  SetPref(network_guid, kLastConnectedTimestampPref, base::Value(timestamp_f));
}

bool NetworkMetadataStore::GetIsConfiguredBySync(
    const std::string& network_guid) {
  const base::Value* is_from_sync = GetPref(network_guid, kIsFromSync);
  if (!is_from_sync) {
    return false;
  }

  return is_from_sync->GetBool();
}

bool NetworkMetadataStore::GetIsCreatedByUser(const std::string& network_guid) {
  const base::Value* owner = GetPref(network_guid, kOwner);
  if (!owner) {
    return false;
  }

  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!user) {
    return false;
  }

  return owner->GetString() == user->username_hash();
}

void NetworkMetadataStore::SetPref(const std::string& network_guid,
                                   const std::string& key,
                                   base::Value value) {
  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(network_guid);

  if (!network)
    return;

  if (network->IsPrivate() && profile_pref_service_) {
    base::Value profile_dict =
        profile_pref_service_->GetDictionary(kNetworkMetadataPref)->Clone();
    profile_dict.SetPath(GetPath(network_guid, key), std::move(value));
    profile_pref_service_->Set(kNetworkMetadataPref, profile_dict);
    return;
  }

  base::Value device_dict =
      device_pref_service_->GetDictionary(kNetworkMetadataPref)->Clone();
  device_dict.SetPath(GetPath(network_guid, key), std::move(value));
  device_pref_service_->Set(kNetworkMetadataPref, device_dict);
}

const base::Value* NetworkMetadataStore::GetPref(
    const std::string& network_guid,
    const std::string& key) {
  if (!network_state_handler_) {
    return nullptr;
  }

  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(network_guid);

  if (!network) {
    return nullptr;
  }

  if (network->IsPrivate() && profile_pref_service_) {
    const base::Value* profile_dict =
        profile_pref_service_->GetDictionary(kNetworkMetadataPref);
    return profile_dict->FindPath(GetPath(network_guid, key));
  }

  const base::Value* device_dict =
      device_pref_service_->GetDictionary(kNetworkMetadataPref);
  return device_dict->FindPath(GetPath(network_guid, key));
}

void NetworkMetadataStore::AddObserver(NetworkMetadataObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkMetadataStore::RemoveObserver(NetworkMetadataObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace chromeos
