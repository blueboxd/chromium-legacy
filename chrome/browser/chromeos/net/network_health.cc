// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health.h"

#include <vector>

#include "chromeos/network/network_event_log.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"

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
}

void NetworkHealth::OnActiveNetworksReceived(
    std::vector<NetworkStatePropertiesPtr> network_props) {
  std::vector<NetworkState> active_networks;
  for (const auto& prop : network_props) {
    DCHECK(prop);
    NetworkState state;
    state.name = prop->name;
    state.type = prop->type;
    state.connection_state = prop->connection_state;
    active_networks.push_back(std::move(state));
  }

  network_health_state_.active_networks.swap(active_networks);
}

void NetworkHealth::OnActiveNetworksChanged(
    std::vector<NetworkStatePropertiesPtr> network_props) {
  OnActiveNetworksReceived(std::move(network_props));
}

void NetworkHealth::RequestActiveNetworks() {
  remote_cros_network_config_->GetNetworkStateList(
      NetworkFilter::New(FilterType::kActive, NetworkType::kAll,
                         network_config::mojom::kNoLimit),
      base::BindOnce(&NetworkHealth::OnActiveNetworksReceived,
                     base::Unretained(this)));
}

}  // namespace chromeos
