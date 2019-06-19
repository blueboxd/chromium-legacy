// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace chromeos {

class NetworkDeviceHandler;
class NetworkStateHandler;

namespace network_config {

class CrosNetworkConfig : public mojom::CrosNetworkConfig,
                          public NetworkStateHandlerObserver {
 public:
  CrosNetworkConfig(NetworkStateHandler* network_state_handler,
                    NetworkDeviceHandler* network_device_handler);
  ~CrosNetworkConfig() override;

  void BindRequest(mojom::CrosNetworkConfigRequest request);

  // mojom::CrosNetworkConfig
  void AddObserver(mojom::CrosNetworkConfigObserverPtr observer) override;
  void GetNetworkState(const std::string& guid,
                       GetNetworkStateCallback callback) override;
  void GetNetworkStateList(mojom::NetworkFilterPtr filter,
                           GetNetworkStateListCallback callback) override;
  void GetDeviceStateList(GetDeviceStateListCallback callback) override;
  void SetNetworkTypeEnabledState(
      mojom::NetworkType type,
      bool enabled,
      SetNetworkTypeEnabledStateCallback callback) override;
  void SetCellularSimState(mojom::CellularSimStatePtr sim_state,
                           SetCellularSimStateCallback callback) override;
  void RequestNetworkScan(mojom::NetworkType type) override;

 private:
  void SetCellularSimStateSuccess(int callback_id);
  void SetCellularSimStateFailure(
      int callback_id,
      const std::string& error_name,
      std::unique_ptr<base::DictionaryValue> error_data);

  // NetworkStateHandlerObserver
  void NetworkListChanged() override;
  void DeviceListChanged() override;
  void ActiveNetworksChanged(
      const std::vector<const NetworkState*>& active_networks) override;
  void DevicePropertiesUpdated(const DeviceState* device) override;
  void OnShuttingDown() override;

  mojom::NetworkStatePropertiesPtr GetMojoNetworkState(
      const NetworkState* network);

  NetworkStateHandler* network_state_handler_;    // Unowned
  NetworkDeviceHandler* network_device_handler_;  // Unowned
  mojo::InterfacePtrSet<mojom::CrosNetworkConfigObserver> observers_;
  mojo::BindingSet<mojom::CrosNetworkConfig> bindings_;
  base::flat_map<int, SetCellularSimStateCallback>
      set_cellular_sim_state_callbacks_;
  int set_cellular_sim_state_callback_id_ = 1;
  base::WeakPtrFactory<CrosNetworkConfig> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkConfig);
};

}  // namespace network_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_CROS_NETWORK_CONFIG_H_
