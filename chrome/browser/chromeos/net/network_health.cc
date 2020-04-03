// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health.h"

#include <vector>

#include "chromeos/network/network_event_log.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

using chromeos::network_config::mojom::DeviceStatePropertiesPtr;
using chromeos::network_config::mojom::FilterType;
using chromeos::network_config::mojom::NetworkFilter;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

namespace chromeos {

NetworkHealth::NetworkHealthState::NetworkHealthState() {}

NetworkHealth::NetworkHealthState::~NetworkHealthState() {}

NetworkHealth::NetworkHealthState::NetworkHealthState(
    const NetworkHealth::NetworkHealthState&) = default;

NetworkHealth::NetworkState::NetworkState() {}

NetworkHealth::NetworkState::~NetworkState() {}

NetworkHealth::DeviceState::DeviceState() {}

NetworkHealth::DeviceState::~DeviceState() {}

NetworkHealth::NetworkHealth() {
  chromeos::network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  RefreshNetworkHealthState();
}

NetworkHealth::~NetworkHealth() {}

NetworkHealth::NetworkHealthState NetworkHealth::GetNetworkHealthState() {
  NET_LOG(EVENT) << "Network Health State Requested";
  return network_health_state_;
}

void NetworkHealth::RefreshNetworkHealthState() {
  RequestActiveNetworks();
  RequestDeviceStateList();
}

void NetworkHealth::OnActiveNetworksReceived(
    std::vector<NetworkStatePropertiesPtr> network_props) {
  std::vector<NetworkState> active_networks;
  for (const auto& prop : network_props) {
    NetworkState state;
    state.name = prop->name;
    state.type = prop->type;
    state.connection_state = prop->connection_state;
    active_networks.push_back(std::move(state));
  }

  network_health_state_.active_networks.swap(active_networks);
}

void NetworkHealth::OnDeviceStateListReceived(
    std::vector<DeviceStatePropertiesPtr> device_props) {
  std::vector<DeviceState> devices;
  for (const auto& prop : device_props) {
    DeviceState device;
    device.mac_address = prop->mac_address.value_or("");
    device.type = prop->type;
    device.state = prop->device_state;
    devices.push_back(std::move(device));
  }

  network_health_state_.devices.swap(devices);
}

void NetworkHealth::OnActiveNetworksChanged(
    std::vector<NetworkStatePropertiesPtr> network_props) {
  OnActiveNetworksReceived(std::move(network_props));
}

void NetworkHealth::OnDeviceStateListChanged() {
  RequestDeviceStateList();
}

void NetworkHealth::RequestActiveNetworks() {
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll,
                         network_config::mojom::kNoLimit),
      base::BindOnce(&NetworkHealth::OnActiveNetworksReceived,
                     base::Unretained(this)));
}

void NetworkHealth::RequestDeviceStateList() {
  remote_cros_network_config_->GetDeviceStateList(base::BindOnce(
      &NetworkHealth::OnDeviceStateListReceived, base::Unretained(this)));
}

}  // namespace chromeos
