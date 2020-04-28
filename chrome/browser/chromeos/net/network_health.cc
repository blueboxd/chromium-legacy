// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health.h"

#include <vector>

#include "chrome/browser/chromeos/net/mojom/network_health.mojom.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

namespace chromeos {
namespace network_health {

NetworkHealth::NetworkHealthState::NetworkHealthState() {}

NetworkHealth::NetworkHealthState::~NetworkHealthState() {}

NetworkHealth::NetworkHealth() {
  network_config::BindToInProcessInstance(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  remote_cros_network_config_->AddObserver(
      cros_network_config_observer_receiver_.BindNewPipeAndPassRemote());
  RefreshNetworkHealthState();
}

NetworkHealth::~NetworkHealth() {}

const NetworkHealth::NetworkHealthState&
NetworkHealth::GetNetworkHealthState() {
  NET_LOG(EVENT) << "Network Health State Requested";
  return network_health_state_;
}

void NetworkHealth::RefreshNetworkHealthState() {
  RequestActiveNetworks();
  RequestDeviceStateList();
}

void NetworkHealth::GetDeviceList(GetDeviceListCallback callback) {
  std::move(callback).Run(mojo::Clone(network_health_state_.devices));
}

void NetworkHealth::GetActiveNetworkList(
    GetActiveNetworkListCallback callback) {
  std::move(callback).Run(mojo::Clone(network_health_state_.active_networks));
}

void NetworkHealth::OnActiveNetworksReceived(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr>
        network_props) {
  std::vector<mojom::NetworkPtr> active_networks;
  for (const auto& prop : network_props) {
    active_networks.push_back(
        mojom::Network::New(prop->connection_state, prop->name, prop->type));
  }
  network_health_state_.active_networks.swap(active_networks);
}

void NetworkHealth::OnDeviceStateListReceived(
    std::vector<network_config::mojom::DeviceStatePropertiesPtr> device_props) {
  std::vector<mojom::DevicePtr> devices;
  for (const auto& prop : device_props) {
    devices.push_back(mojom::Device::New(
        prop->device_state, prop->mac_address.value_or(""), prop->type));
  }
  network_health_state_.devices.swap(devices);
}

void NetworkHealth::OnActiveNetworksChanged(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr>
        network_props) {
  OnActiveNetworksReceived(std::move(network_props));
}

void NetworkHealth::OnDeviceStateListChanged() {
  RequestDeviceStateList();
}

void NetworkHealth::RequestActiveNetworks() {
  remote_cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kAll,
          network_config::mojom::kNoLimit),
      base::BindOnce(&NetworkHealth::OnActiveNetworksReceived,
                     base::Unretained(this)));
}

void NetworkHealth::RequestDeviceStateList() {
  remote_cros_network_config_->GetDeviceStateList(base::BindOnce(
      &NetworkHealth::OnDeviceStateListReceived, base::Unretained(this)));
}

}  // namespace network_health
}  // namespace chromeos
