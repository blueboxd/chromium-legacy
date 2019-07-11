// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_
#define CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace service_manager {
class Connector;
}

namespace chromeos {

class NetworkDeviceHandler;

namespace network_config {

class CrosNetworkConfig;
class CrosNetworkConfigTestObserver;

class CrosNetworkConfigTestHelper {
 public:
  // Default constructor for unit tests.
  CrosNetworkConfigTestHelper();
  // Constructor used by ash_shell_with_content providing the Shell connector.
  explicit CrosNetworkConfigTestHelper(service_manager::Connector* connector);

  ~CrosNetworkConfigTestHelper();

  // Binds |service_interface_ptr_|. Must be called before using
  // service_interface().
  void SetupServiceInterface();

  // Binds |observer_| to |service_interface_ptr_|. Must be called before using
  // observer(). SetupServiceInterface() must be called first.
  void SetupObserver();

  NetworkStateTestHelper& network_state_helper() {
    return network_state_helper_;
  }
  mojom::CrosNetworkConfig* service_interface_ptr() {
    return service_interface_ptr_.get();
  }
  service_manager::Connector* connector() { return connector_; }
  CrosNetworkConfigTestObserver* observer() { return observer_.get(); }

  void FlushForTesting();

 private:
  void SetupCrosNetworkConfig();
  void SetupService();
  void AddBinding(mojo::ScopedMessagePipeHandle handle);

  NetworkStateTestHelper network_state_helper_{
      false /* use_default_devices_and_services */};
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<CrosNetworkConfig> cros_network_config_impl_;

  // Unowned pointer to the service connector.
  service_manager::Connector* connector_;

  // Owned connector for unit tests. |connector_| is set to the raw pointer.
  std::unique_ptr<service_manager::Connector> owned_connector_;

  // Interface to |cros_network_config_| through service connector.
  mojom::CrosNetworkConfigPtr service_interface_ptr_;

  std::unique_ptr<CrosNetworkConfigTestObserver> observer_;

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkConfigTestHelper);
};

}  // namespace network_config
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_NETWORK_CONFIG_PUBLIC_CPP_CROS_NETWORK_CONFIG_TEST_HELPER_H_
