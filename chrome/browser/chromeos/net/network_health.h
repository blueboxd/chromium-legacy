// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_H_
#define CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_H_

#include <string>
#include <vector>

#include "chrome/browser/chromeos/net/mojom/network_health.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {
namespace network_health {

class NetworkHealth : public mojom::NetworkHealthService,
                      public network_config::mojom::CrosNetworkConfigObserver {
 public:
  NetworkHealth();

  ~NetworkHealth() override;

  // Returns the current NetworkHealthState.
  const mojom::NetworkHealthStatePtr GetNetworkHealthState();

  // Handler for receiving the network state list.
  void OnNetworkStateListReceived(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr>);

  // Handler for receiving networking devices.
  void OnDeviceStateListReceived(
      std::vector<network_config::mojom::DeviceStatePropertiesPtr>);

  // NetworkHealthService implementation
  void GetNetworkList(GetNetworkListCallback) override;
  void GetHealthSnapshot(GetHealthSnapshotCallback) override;

  // CrosNetworkConfigObserver implementation
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;

  // CrosNetworkConfigObserver unimplemented callbacks
  void OnActiveNetworksChanged(
      std::vector<network_config::mojom::NetworkStatePropertiesPtr>) override {}
  void OnNetworkStateChanged(
      network_config::mojom::NetworkStatePropertiesPtr) override {}
  void OnVpnProvidersChanged() override {}
  void OnNetworkCertificatesChanged() override {}

 private:
  // Creates the NetworkHealthState structure from cached network information.
  void CreateNetworkHealthState();

  // Asynchronous call that refreshes the current Network Health State.
  void RefreshNetworkHealthState();
  void RequestNetworkStateList();
  void RequestDeviceStateList();

  mojo::Remote<network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  mojom::NetworkHealthState network_health_state_;

  std::vector<network_config::mojom::DeviceStatePropertiesPtr>
      device_properties_;
  std::vector<network_config::mojom::NetworkStatePropertiesPtr>
      network_properties_;
};

}  // namespace network_health
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_NETWORK_HEALTH_H_
