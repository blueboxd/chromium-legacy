// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health.h"

#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using chromeos::network_config::mojom::NetworkType;

namespace {

// Constant values for fake device and service.
constexpr char kServicePath[] = "/service/0";
constexpr char kServiceName[] = "service_name";
constexpr char kServiceGUID[] = "eth_guid";
constexpr char kDevicePath[] = "/device/eth";
constexpr char kDeviceName[] = "device_name";

}  // namespace

namespace chromeos {
namespace network_health {

class NetworkHealthTest : public ::testing::Test {
 public:
  NetworkHealthTest() {
    // Wait until CrosNetworkConfigTestHelper is fully setup.
    task_environment_.RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_;
  NetworkHealth network_health_;
};

TEST_F(NetworkHealthTest, GetNetworkHealthSucceeds) {
  // Check that the default wifi device created by CrosNetworkConfigTestHelper
  // exists.
  const auto& initial_network_health_state =
      network_health_.GetNetworkHealthState();
  ASSERT_EQ(initial_network_health_state.devices.size(), std::size_t(1));

  // Create an ethernet device and service, and make it the active network.
  cros_network_config_test_helper_.network_state_helper().AddDevice(
      kDevicePath, shill::kTypeEthernet, kDeviceName);

  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kServicePath, kServiceGUID, kServiceName,
                   shill::kTypeEthernet, shill::kStateOnline, true);

  // Wait until the network and service have been created and configured.
  task_environment_.RunUntilIdle();

  const auto& network_health_state = network_health_.GetNetworkHealthState();
  ASSERT_EQ(network_health_state.devices.size(), std::size_t(2));
  ASSERT_EQ(network_health_state.active_networks.size(), std::size_t(1));
  EXPECT_EQ(network_health_state.active_networks[0]->name, kServiceName);
  EXPECT_EQ(network_health_state.devices[1]->type, NetworkType::kEthernet);
}

}  // namespace network_health
}  // namespace chromeos
