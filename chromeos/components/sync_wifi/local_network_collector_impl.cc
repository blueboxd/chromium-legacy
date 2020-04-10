// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/local_network_collector_impl.h"

#include "base/guid.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/components/sync_wifi/network_type_conversions.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/device_event_log/device_event_log.h"
#include "components/sync/protocol/wifi_configuration_specifics.pb.h"
#include "dbus/object_path.h"

namespace chromeos {

namespace sync_wifi {

namespace {

dbus::ObjectPath GetServicePathForGuid(const std::string& guid) {
  const NetworkState* state =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!state) {
    return dbus::ObjectPath();
  }

  return dbus::ObjectPath(state->path());
}

}  // namespace

LocalNetworkCollectorImpl::LocalNetworkCollectorImpl(
    network_config::mojom::CrosNetworkConfig* cros_network_config)
    : cros_network_config_(cros_network_config) {
  cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());

  // Load the current list of networks.
  OnNetworkStateListChanged();
}

LocalNetworkCollectorImpl::~LocalNetworkCollectorImpl() = default;

void LocalNetworkCollectorImpl::GetAllSyncableNetworks(
    ProtoListCallback callback) {
  std::string request_guid = InitializeRequest();
  request_guid_to_list_callback_[request_guid] = std::move(callback);

  int count = 0;
  for (const network_config::mojom::NetworkStatePropertiesPtr& network :
       mojo_networks_) {
    if (!IsEligible(network)) {
      continue;
    }

    request_guid_to_in_flight_networks_[request_guid].insert(
        NetworkIdentifier::FromMojoNetwork(network));
    StartGetNetworkDetails(network.get(), request_guid);
    count++;
  }

  if (!count) {
    OnRequestFinished(request_guid);
  }
}

void LocalNetworkCollectorImpl::GetSyncableNetwork(const std::string& guid,
                                                   ProtoCallback callback) {
  const network_config::mojom::NetworkStateProperties* network = nullptr;
  for (const network_config::mojom::NetworkStatePropertiesPtr& n :
       mojo_networks_) {
    if (n->guid == guid) {
      if (IsEligible(n)) {
        network = n.get();
      }

      break;
    }
  }

  if (!network) {
    std::move(callback).Run(base::nullopt);
    return;
  }

  std::string request_guid = InitializeRequest();
  request_guid_to_single_callback_[request_guid] = std::move(callback);

  StartGetNetworkDetails(network, request_guid);
}

base::Optional<NetworkIdentifier>
LocalNetworkCollectorImpl::GetNetworkIdentifierFromGuid(
    const std::string& guid) {
  for (const network_config::mojom::NetworkStatePropertiesPtr& network :
       mojo_networks_) {
    if (network->guid == guid) {
      return NetworkIdentifier::FromMojoNetwork(network);
    }
  }
  return base::nullopt;
}

void LocalNetworkCollectorImpl::SetNetworkMetadataStore(
    base::WeakPtr<NetworkMetadataStore> network_metadata_store) {
  network_metadata_store_ = network_metadata_store;
}

std::string LocalNetworkCollectorImpl::InitializeRequest() {
  std::string request_guid = base::GenerateGUID();
  request_guid_to_complete_protos_[request_guid] =
      std::vector<sync_pb::WifiConfigurationSpecifics>();
  request_guid_to_in_flight_networks_[request_guid] =
      base::flat_set<NetworkIdentifier>();
  return request_guid;
}

bool LocalNetworkCollectorImpl::IsEligible(
    const network_config::mojom::NetworkStatePropertiesPtr& network) {
  if (!network) {
    return false;
  }

  if (network->type != network_config::mojom::NetworkType::kWiFi) {
    return false;
  }

  if (!network->connectable) {
    NET_LOG(EVENT) << NetworkGuidId(network->guid)
                   << " is not eligible, it is not connectable.";
    return false;
  }

  if (network->source != network_config::mojom::OncSource::kUser &&
      !network_metadata_store_->GetIsCreatedByUser(network->guid)) {
    NET_LOG(EVENT) << NetworkGuidId(network->guid)
                   << " is not eligible, was not configured by user.";
    return false;
  }

  const network_config::mojom::WiFiStatePropertiesPtr& wifi_properties =
      network->type_state->get_wifi();
  if (wifi_properties->security !=
          network_config::mojom::SecurityType::kWepPsk &&
      wifi_properties->security !=
          network_config::mojom::SecurityType::kWpaPsk) {
    NET_LOG(EVENT) << NetworkGuidId(network->guid)
                   << " is not eligible, security type not supported: "
                   << wifi_properties->security;
    return false;
  }

  base::TimeDelta timestamp =
      network_metadata_store_->GetLastConnectedTimestamp(network->guid);
  if (timestamp.is_zero()) {
    NET_LOG(EVENT) << NetworkGuidId(network->guid)
                   << " is not eligible, never connected.";
    return false;
  }

  NET_LOG(EVENT) << NetworkGuidId(network->guid) << " is eligible for sync.";
  return true;
}

void LocalNetworkCollectorImpl::StartGetNetworkDetails(
    const network_config::mojom::NetworkStateProperties* network,
    const std::string& request_guid) {
  sync_pb::WifiConfigurationSpecifics proto;
  proto.set_hex_ssid(network->type_state->get_wifi()->hex_ssid);
  proto.set_security_type(
      SecurityTypeProtoFromMojo(network->type_state->get_wifi()->security));
  base::TimeDelta timestamp =
      network_metadata_store_->GetLastConnectedTimestamp(network->guid);
  proto.set_last_connected_timestamp(timestamp.InMilliseconds());
  cros_network_config_->GetManagedProperties(
      network->guid,
      base::BindOnce(&LocalNetworkCollectorImpl::OnGetManagedPropertiesResult,
                     weak_ptr_factory_.GetWeakPtr(), proto, request_guid));
}

void LocalNetworkCollectorImpl::OnGetManagedPropertiesResult(
    sync_pb::WifiConfigurationSpecifics proto,
    const std::string& request_guid,
    network_config::mojom::ManagedPropertiesPtr properties) {
  if (!properties) {
    NET_LOG(ERROR) << "GetManagedProperties failed.";
    OnNetworkFinished(NetworkIdentifier::FromProto(proto), request_guid);
    return;
  }
  proto.set_automatically_connect(AutomaticallyConnectProtoFromMojo(
      properties->type_properties->get_wifi()->auto_connect));
  proto.set_is_preferred(IsPreferredProtoFromMojo(properties->priority));
  proto.mutable_proxy_configuration()->CopyFrom(
      ProxyConfigurationProtoFromMojo(properties->proxy_settings));

  if (properties->static_ip_config &&
      properties->static_ip_config->name_servers) {
    for (const std::string& nameserver :
         properties->static_ip_config->name_servers->active_value) {
      proto.add_custom_dns(nameserver);
    }
  }

  ShillServiceClient::Get()->GetWiFiPassphrase(
      GetServicePathForGuid(properties->guid),
      base::BindOnce(&LocalNetworkCollectorImpl::OnGetWiFiPassphraseResult,
                     weak_ptr_factory_.GetWeakPtr(), proto, request_guid),
      base::BindOnce(&LocalNetworkCollectorImpl::OnGetWiFiPassphraseError,
                     weak_ptr_factory_.GetWeakPtr(),
                     NetworkIdentifier::FromProto(proto), request_guid));
}

void LocalNetworkCollectorImpl::OnGetWiFiPassphraseResult(
    sync_pb::WifiConfigurationSpecifics proto,
    const std::string& request_guid,
    const std::string& passphrase) {
  proto.set_passphrase(passphrase);
  NetworkIdentifier id = NetworkIdentifier::FromProto(proto);
  request_guid_to_complete_protos_[request_guid].push_back(std::move(proto));
  OnNetworkFinished(id, request_guid);
}

void LocalNetworkCollectorImpl::OnGetWiFiPassphraseError(
    const NetworkIdentifier& id,
    const std::string& request_guid,
    const std::string& error_name,
    const std::string& error_message) {
  NET_LOG(ERROR) << "Failed to get passphrase for " << id.SerializeToString()
                 << " Error: " << error_name << " Details: " << error_message;

  OnNetworkFinished(id, request_guid);
}

void LocalNetworkCollectorImpl::OnNetworkFinished(
    const NetworkIdentifier& id,
    const std::string& request_guid) {
  request_guid_to_in_flight_networks_[request_guid].erase(id);

  if (request_guid_to_in_flight_networks_[request_guid].empty()) {
    OnRequestFinished(request_guid);
  }
}

void LocalNetworkCollectorImpl::OnRequestFinished(
    const std::string& request_guid) {
  DCHECK(request_guid_to_in_flight_networks_[request_guid].empty());

  if (request_guid_to_single_callback_[request_guid]) {
    std::vector<sync_pb::WifiConfigurationSpecifics>& list =
        request_guid_to_complete_protos_[request_guid];
    DCHECK(list.size() <= 1);
    base::Optional<sync_pb::WifiConfigurationSpecifics> result;
    if (list.size() == 1) {
      result = list[0];
    }
    std::move(request_guid_to_single_callback_[request_guid]).Run(result);
    request_guid_to_single_callback_.erase(request_guid);
  } else {
    ProtoListCallback callback =
        std::move(request_guid_to_list_callback_[request_guid]);
    DCHECK(callback);
    std::move(callback).Run(request_guid_to_complete_protos_[request_guid]);
    request_guid_to_list_callback_.erase(request_guid);
  }

  request_guid_to_complete_protos_.erase(request_guid);
  request_guid_to_in_flight_networks_.erase(request_guid);
}

void LocalNetworkCollectorImpl::OnNetworkStateListChanged() {
  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kConfigured,
          network_config::mojom::NetworkType::kWiFi,
          /*limit=*/0),
      base::BindOnce(&LocalNetworkCollectorImpl::OnGetNetworkList,
                     weak_ptr_factory_.GetWeakPtr()));
}

void LocalNetworkCollectorImpl::OnGetNetworkList(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  mojo_networks_ = std::move(networks);
}

}  // namespace sync_wifi

}  // namespace chromeos
