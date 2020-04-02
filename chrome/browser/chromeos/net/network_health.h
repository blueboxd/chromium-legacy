// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_H_

#include <string>
#include <vector>

#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

class NetworkHealth
    : public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  // Structure for a single network's status.
  struct NetworkState {
    NetworkState();
    ~NetworkState();

    std::string name;
    chromeos::network_config::mojom::NetworkType type;
    chromeos::network_config::mojom::ConnectionStateType connection_state;
  };

  // Structure containing the current snapshot of the state of Network Health.
  struct NetworkHealthState {
    NetworkHealthState();
    NetworkHealthState(const NetworkHealthState&);
    ~NetworkHealthState();

    std::vector<NetworkState> active_networks;
  };

  NetworkHealth();

  ~NetworkHealth() override;

  // Returns the current NetworkHealthState.
  NetworkHealthState GetNetworkHealthState();

  // Handler for receiving new active networks.
  void OnActiveNetworksReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>);

  // CrosNetworkConfigObserver implementation
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>)
      override;

  // CrosNetworkConfigObserver unimplemented callbacks
  void OnNetworkStateListChanged() override {}
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr) override {}
  void OnDeviceStateListChanged() override {}
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}

 private:
  // Asynchronous call that refreshes the current Network Health State.
  void RefreshNetworkHealthState();
  void RequestActiveNetworks();

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  NetworkHealthState network_health_state_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_H_
