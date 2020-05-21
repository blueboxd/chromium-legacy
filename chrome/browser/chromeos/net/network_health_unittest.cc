// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_health.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/net/mojom/network_health.mojom.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using chromeos::network_config::mojom::NetworkType;

namespace {

// Constant values for fake devices and services.
constexpr char kEthServicePath[] = "/service/eth/0";
constexpr char kEthServiceName[] = "eth_service_name";
constexpr char kEthGuid[] = "eth_guid";
constexpr char kEthDevicePath[] = "/device/eth1";
constexpr char kEthName[] = "eth-name";
constexpr char kWifiServicePath[] = "service/wifi/0";
constexpr char kWifiServiceName[] = "wifi_service_name";
constexpr char kWifiGuid[] = "wifi_guid";
constexpr char kWifiDevicePath[] = "/device/wifi1";
constexpr char kWifiName[] = "wifi_device1";

}  // namespace

namespace chromeos {
namespace network_health {

class NetworkHealthTest : public ::testing::Test {
 public:
  NetworkHealthTest() {
    // Wait until CrosNetworkConfigTestHelper is fully setup.
    task_environment_.RunUntilIdle();
  }

  void SetUp() override {
    // Remove the default WiFi device created by network_state_helper.
    cros_network_config_test_helper_.network_state_helper().ClearDevices();
    cros_network_config_test_helper_.network_state_helper()
        .manager_test()
        ->RemoveTechnology(shill::kTypeWifi);
    task_environment_.RunUntilIdle();
  }

  void CreateDefaultWifiDevice() {
    // Reset the network_state_helper to include the default wifi device.
    cros_network_config_test_helper_.network_state_helper()
        .ResetDevicesAndServices();
    task_environment_.RunUntilIdle();

    // Check that the default wifi device created by CrosNetworkConfigTestHelper
    // exists.
    const auto& initial_network_health_state =
        network_health_.GetNetworkHealthState();
    ASSERT_EQ(initial_network_health_state->networks.size(), std::size_t(1));
    ASSERT_EQ(initial_network_health_state->networks[0]->type,
              network_config::mojom::NetworkType::kWiFi);
    ASSERT_EQ(initial_network_health_state->networks[0]->state,
              network_health::mojom::NetworkState::kNotConnected);
  }

  void ValidateState(network_health::mojom::NetworkState expected_state) {
    task_environment_.RunUntilIdle();
    const auto& network_health_state = network_health_.GetNetworkHealthState();
    ASSERT_EQ(network_health_state->networks.size(), std::size_t(1));
    EXPECT_EQ(network_health_state->networks[0]->state, expected_state);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_test_helper_;
  NetworkHealth network_health_;
};

// Test that all Network states can be represented by NetworkHealth.

TEST_F(NetworkHealthTest, NetworkStateUninitialized) {
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->AddTechnology(shill::kTypeWifi, false);
  cros_network_config_test_helper_.network_state_helper()
      .device_test()
      ->AddDevice(kWifiDevicePath, shill::kTypeWifi, kWifiName);
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->SetTechnologyInitializing(shill::kTypeWifi, true);

  ValidateState(network_health::mojom::NetworkState::kUninitialized);
}

TEST_F(NetworkHealthTest, NetworkStateDisabled) {
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->AddTechnology(shill::kTypeWifi, false);
  cros_network_config_test_helper_.network_state_helper()
      .device_test()
      ->AddDevice(kWifiDevicePath, shill::kTypeWifi, kWifiName);
  ValidateState(network_health::mojom::NetworkState::kDisabled);
}

// TODO(crbug/1081488): implement test for kEnabling state.

TEST_F(NetworkHealthTest, NetworkStateProhibited) {
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->AddTechnology(shill::kTypeWifi, true);
  cros_network_config_test_helper_.network_state_helper()
      .device_test()
      ->AddDevice(kWifiDevicePath, shill::kTypeWifi, kWifiName);
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->SetTechnologyProhibited(shill::kTypeWifi, true);
  ValidateState(network_health::mojom::NetworkState::kProhibited);
}

TEST_F(NetworkHealthTest, NetworkStateNotConnected) {
  cros_network_config_test_helper_.network_state_helper()
      .manager_test()
      ->AddTechnology(shill::kTypeWifi, true);
  cros_network_config_test_helper_.network_state_helper()
      .device_test()
      ->AddDevice(kWifiDevicePath, shill::kTypeWifi, kWifiName);
  ValidateState(network_health::mojom::NetworkState::kNotConnected);
}

TEST_F(NetworkHealthTest, NetworkStateConnecting) {
  CreateDefaultWifiDevice();
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiDevicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateAssociation, true);
  ValidateState(network_health::mojom::NetworkState::kConnecting);
}

TEST_F(NetworkHealthTest, NetworkStatePortal) {
  CreateDefaultWifiDevice();
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiDevicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStatePortal, true);
  ValidateState(network_health::mojom::NetworkState::kPortal);
}

TEST_F(NetworkHealthTest, NetworkStateConnected) {
  CreateDefaultWifiDevice();
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiDevicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateReady, true);
  ValidateState(network_health::mojom::NetworkState::kConnected);
}

TEST_F(NetworkHealthTest, OneWifiNetworkConnected) {
  CreateDefaultWifiDevice();
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiDevicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateOnline, true);
  task_environment_.RunUntilIdle();

  const auto& network_health_state = network_health_.GetNetworkHealthState();
  ASSERT_EQ(network_health_state->networks.size(), std::size_t(1));
  EXPECT_EQ(network_health_state->networks[0]->name, kWifiServiceName);
  EXPECT_EQ(network_health_state->networks[0]->type,
            network_config::mojom::NetworkType::kWiFi);
  EXPECT_EQ(network_health_state->networks[0]->state,
            network_health::mojom::NetworkState::kOnline);
}

TEST_F(NetworkHealthTest, MultiWifiNetwork) {
  CreateDefaultWifiDevice();
  // Create multiple wifi networks that are not connected.
  for (int i = 0; i < 3; i++) {
    std::string idx = base::NumberToString(i);
    cros_network_config_test_helper_.network_state_helper()
        .service_test()
        ->AddService(kWifiDevicePath + idx, kWifiGuid + idx,
                     kWifiServiceName + idx, shill::kTypeWifi,
                     shill::kStateIdle, true);
  }

  task_environment_.RunUntilIdle();

  // Check that there is only a single network created for WiFi and is not
  // connected.
  const auto& network_health_state = network_health_.GetNetworkHealthState();
  ASSERT_EQ(network_health_state->networks.size(), std::size_t(1));
  EXPECT_EQ(network_health_state->networks[0]->type,
            network_config::mojom::NetworkType::kWiFi);
  EXPECT_EQ(network_health_state->networks[0]->state,
            network_health::mojom::NetworkState::kNotConnected);
}

TEST_F(NetworkHealthTest, MultiWifiNetworkConnected) {
  CreateDefaultWifiDevice();
  // Create one wifi service that is online.
  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kWifiServicePath, kWifiGuid, kWifiServiceName,
                   shill::kTypeWifi, shill::kStateOnline, true);

  // Create multiple wifi services that are not connected.
  for (int i = 1; i < 3; i++) {
    std::string idx = base::NumberToString(i);
    cros_network_config_test_helper_.network_state_helper()
        .service_test()
        ->AddService(kWifiServicePath + idx, kWifiGuid + idx,
                     kWifiServiceName + idx, shill::kTypeWifi,
                     shill::kStateOffline, true);
  }

  task_environment_.RunUntilIdle();

  // Check that there is only a single network created for WiFi and is
  // connected.
  const auto& network_health_state = network_health_.GetNetworkHealthState();
  ASSERT_EQ(network_health_state->networks.size(), std::size_t(1));
  EXPECT_EQ(network_health_state->networks[0]->type,
            network_config::mojom::NetworkType::kWiFi);
  EXPECT_EQ(network_health_state->networks[0]->state,
            network_health::mojom::NetworkState::kOnline);
  EXPECT_EQ(network_health_state->networks[0]->name, kWifiServiceName);
}

TEST_F(NetworkHealthTest, CreateActiveEthernet) {
  CreateDefaultWifiDevice();
  // Create an ethernet device and service, and make it the active network.
  cros_network_config_test_helper_.network_state_helper().AddDevice(
      kEthDevicePath, shill::kTypeEthernet, kEthName);

  cros_network_config_test_helper_.network_state_helper()
      .service_test()
      ->AddService(kEthServicePath, kEthGuid, kEthServiceName,
                   shill::kTypeEthernet, shill::kStateOnline, true);

  // Wait until the network and service have been created and configured.
  task_environment_.RunUntilIdle();

  const auto& network_health_state = network_health_.GetNetworkHealthState();

  ASSERT_EQ(network_health_state->networks.size(), std::size_t(2));
  // The first network should be the Ethernet device because it is active.
  EXPECT_EQ(network_health_state->networks[0]->name, kEthServiceName);
  EXPECT_EQ(network_health_state->networks[0]->type,
            network_config::mojom::NetworkType::kEthernet);
  EXPECT_EQ(network_health_state->networks[0]->state,
            network_health::mojom::NetworkState::kOnline);

  // The second network should be the default unconnected WiFi device.
  EXPECT_EQ(network_health_state->networks[1]->type,
            network_config::mojom::NetworkType::kWiFi);
  EXPECT_EQ(network_health_state->networks[1]->state,
            mojom::NetworkState::kNotConnected);
}

}  // namespace network_health
}  // namespace chromeos
