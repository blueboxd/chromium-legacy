// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_OBSERVER_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_OBSERVER_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace chromeos {
namespace network_config {

class CrosNetworkConfigTestObserver : public mojom::CrosNetworkConfigObserver {
 public:
  CrosNetworkConfigTestObserver();
  ~CrosNetworkConfigTestObserver() override;

  mojom::CrosNetworkConfigObserverPtr GenerateInterfacePtr();

  // mojom::CrosNetworkConfigObserver:
  void OnActiveNetworksChanged(
      std::vector<mojom::NetworkStatePropertiesPtr> networks) override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network)
      override;
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;

  int active_networks_changed() const { return active_networks_changed_; }
  int network_state_list_changed() const { return network_state_list_changed_; }
  int device_state_list_changed() const { return device_state_list_changed_; }
  int GetNetworkChangedCount(const std::string& guid) const;
  void ResetNetworkChanges();

  mojo::Binding<mojom::CrosNetworkConfigObserver>& binding() {
    return binding_;
  }

  void FlushForTesting();

 private:
  mojo::Binding<mojom::CrosNetworkConfigObserver> binding_{this};
  int active_networks_changed_ = 0;
  std::map<std::string, int> networks_changed_;
  int network_state_list_changed_ = 0;
  int device_state_list_changed_ = 0;

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkConfigTestObserver);
};

}  // namespace network_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_OBSERVER_H_
