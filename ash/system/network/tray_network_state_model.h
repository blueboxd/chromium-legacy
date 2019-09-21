// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_MODEL_H_
#define ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_MODEL_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/system/network/tray_network_state_observer.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/timer/timer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

// TrayNetworkStateModel observes the mojo interface and tracks the devices
// and active networks. It has UI observers that are informed when important
// changes occur.
class ASH_EXPORT TrayNetworkStateModel
    : public chromeos::network_config::mojom::CrosNetworkConfigObserver {
 public:
  TrayNetworkStateModel();
  ~TrayNetworkStateModel() override;

  void AddObserver(TrayNetworkStateObserver* observer);
  void RemoveObserver(TrayNetworkStateObserver* observer);

  // Returns DeviceStateProperties for |type| if it exists or null.
  const chromeos::network_config::mojom::DeviceStateProperties* GetDevice(
      chromeos::network_config::mojom::NetworkType type) const;

  // Returns the DeviceStateType for |type| if a device exists or kUnavailable.
  chromeos::network_config::mojom::DeviceStateType GetDeviceState(
      chromeos::network_config::mojom::NetworkType type);

  // Convenience method to call the |remote_cros_network_config_| method.
  void SetNetworkTypeEnabledState(
      chromeos::network_config::mojom::NetworkType type,
      bool enabled);

  chromeos::network_config::mojom::CrosNetworkConfig* cros_network_config() {
    return remote_cros_network_config_.get();
  }

  const chromeos::network_config::mojom::NetworkStateProperties*
  default_network() const {
    return default_network_.get();
  }
  const chromeos::network_config::mojom::NetworkStateProperties*
  active_non_cellular() const {
    return active_non_cellular_.get();
  }
  const chromeos::network_config::mojom::NetworkStateProperties*
  active_cellular() const {
    return active_cellular_.get();
  }
  const chromeos::network_config::mojom::NetworkStateProperties* active_vpn()
      const {
    return active_vpn_.get();
  }
  bool has_vpn() const { return has_vpn_; }

 private:
  // CrosNetworkConfigObserver
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks) override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override;
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;
  void OnVpnProvidersChanged() override;

  void GetDeviceStateList();
  void OnGetDeviceStateList(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);

  void GetActiveNetworks();
  void UpdateActiveNetworks(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  void GetVirtualNetworks();
  void OnGetVirtualNetworks(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  void NotifyNetworkListChanged();
  void SendActiveNetworkStateChanged();
  void SendNetworkListChanged();

  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  base::ObserverList<TrayNetworkStateObserver> observer_list_;

  // Frequency at which to push NetworkListChanged updates. This avoids
  // unnecessarily frequent UI updates (which can be expensive). We set this
  // to 0 for tests to eliminate timing variance.
  int update_frequency_;

  // Timer used to limit the frequency of NetworkListChanged updates.
  base::OneShotTimer timer_;

  base::flat_map<chromeos::network_config::mojom::NetworkType,
                 chromeos::network_config::mojom::DeviceStatePropertiesPtr>
      devices_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr default_network_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr
      active_non_cellular_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr active_cellular_;
  chromeos::network_config::mojom::NetworkStatePropertiesPtr active_vpn_;
  bool has_vpn_ = false;

  DISALLOW_COPY_AND_ASSIGN(TrayNetworkStateModel);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_TRAY_NETWORK_STATE_MODEL_H_
