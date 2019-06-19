// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/network_config/cros_network_config.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/dbus/shill/fake_shill_device_client.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_test_helper.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_observer.h"
#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace network_config {

namespace {

const int kSimRetriesLeft = 3;
const char* kCellularDevicePath = "/device/stub_cellular_device";

}  // namespace

class CrosNetworkConfigTest : public testing::Test {
 public:
  CrosNetworkConfigTest() {
    network_device_handler_ = NetworkDeviceHandler::InitializeForTesting(
        helper_.network_state_handler());
    cros_network_config_ = std::make_unique<CrosNetworkConfig>(
        helper_.network_state_handler(), network_device_handler_.get());
    SetupNetworks();
  }

  ~CrosNetworkConfigTest() override { cros_network_config_.reset(); }

  void SetupNetworks() {
    // Wifi device exists by default, add Ethernet and Cellular.
    helper().device_test()->AddDevice("/device/stub_eth_device",
                                      shill::kTypeEthernet, "stub_eth_device");
    helper().manager_test()->AddTechnology(shill::kTypeCellular,
                                           true /* enabled */);
    helper().device_test()->AddDevice(kCellularDevicePath, shill::kTypeCellular,
                                      "stub_cellular_device");
    base::Value sim_value(base::Value::Type::DICTIONARY);
    sim_value.SetKey(shill::kSIMLockEnabledProperty, base::Value(true));
    sim_value.SetKey(shill::kSIMLockTypeProperty,
                     base::Value(shill::kSIMLockPin));
    sim_value.SetKey(shill::kSIMLockRetriesLeftProperty,
                     base::Value(kSimRetriesLeft));
    helper().device_test()->SetDeviceProperty(
        kCellularDevicePath, shill::kSIMLockStatusProperty, sim_value,
        /*notify_changed=*/false);

    helper().ConfigureService(
        R"({"GUID": "eth_guid", "Type": "ethernet", "State": "online"})");
    wifi1_path_ = helper().ConfigureService(
        R"({"GUID": "wifi1_guid", "Type": "wifi", "State": "ready",
            "Strength": 50})");
    helper().ConfigureService(
        R"({"GUID": "wifi2_guid", "Type": "wifi", "State": "idle",
            "SecurityClass": "psk", "Strength": 100})");
    helper().ConfigureService(
        R"({"GUID": "cellular_guid", "Type": "cellular",  "State": "idle",
            "Strength": 0, "Cellular.NetworkTechnology": "LTE",
            "Cellular.ActivationState": "activated"})");
    helper().ConfigureService(
        R"({"GUID": "vpn_guid", "Type": "vpn", "State": "association",
            "Provider": {"Type": "l2tpipsec"}})");

    // Add a non visible configured wifi service.
    std::string wifi3_path = helper().ConfigureService(
        R"({"GUID": "wifi3_guid", "Type": "wifi", "Visible": false})");
    helper().profile_test()->AddService(helper().ProfilePathShared(),
                                        wifi3_path);
    base::RunLoop().RunUntilIdle();
  }

  void SetupObserver() {
    observer_ = std::make_unique<CrosNetworkConfigTestObserver>();
    cros_network_config_->AddObserver(observer_->GenerateInterfacePtr());
  }

  mojom::DeviceStatePropertiesPtr GetDeviceState(mojom::NetworkType type) {
    mojom::DeviceStatePropertiesPtr result;
    base::RunLoop run_loop;
    cros_network_config()->GetDeviceStateList(base::BindOnce(
        [](mojom::NetworkType type, mojom::DeviceStatePropertiesPtr* result,
           base::OnceClosure quit_closure,
           std::vector<mojom::DeviceStatePropertiesPtr> devices) {
          for (auto& device : devices) {
            if (device->type == type) {
              *result = std::move(device);
              std::move(quit_closure).Run();
              return;
            }
          }
        },
        type, &result, run_loop.QuitClosure()));
    run_loop.Run();
    return result;
  }

  bool SetCellularSimState(const std::string& current_pin_or_puk,
                           base::Optional<std::string> new_pin,
                           bool require_pin) {
    bool success = false;
    base::RunLoop run_loop;
    cros_network_config()->SetCellularSimState(
        mojom::CellularSimState::New(current_pin_or_puk, new_pin, require_pin),
        base::BindOnce(
            [](bool* successp, base::OnceClosure quit_closure, bool success) {
              *successp = success;
              std::move(quit_closure).Run();
            },
            &success, run_loop.QuitClosure()));
    run_loop.Run();
    return success;
  }

  NetworkStateTestHelper& helper() { return helper_; }
  CrosNetworkConfigTestObserver* observer() { return observer_.get(); }
  CrosNetworkConfig* cros_network_config() {
    return cros_network_config_.get();
  }
  std::string wifi1_path() { return wifi1_path_; }

 private:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  NetworkStateTestHelper helper_{false /* use_default_devices_and_services */};
  std::unique_ptr<NetworkDeviceHandler> network_device_handler_;
  std::unique_ptr<CrosNetworkConfig> cros_network_config_;
  std::unique_ptr<CrosNetworkConfigTestObserver> observer_;
  std::string wifi1_path_;

  DISALLOW_COPY_AND_ASSIGN(CrosNetworkConfigTest);
};

TEST_F(CrosNetworkConfigTest, GetNetworkState) {
  cros_network_config()->GetNetworkState(
      "eth_guid", base::BindOnce([](mojom::NetworkStatePropertiesPtr result) {
        ASSERT_TRUE(result);
        EXPECT_EQ("eth_guid", result->guid);
        EXPECT_EQ(mojom::NetworkType::kEthernet, result->type);
        EXPECT_EQ(mojom::ConnectionStateType::kOnline,
                  result->connection_state);
        EXPECT_EQ(mojom::ONCSource::kUnknown, result->source);
      }));
  cros_network_config()->GetNetworkState(
      "wifi1_guid", base::BindOnce([](mojom::NetworkStatePropertiesPtr result) {
        ASSERT_TRUE(result);
        EXPECT_EQ("wifi1_guid", result->guid);
        EXPECT_EQ(mojom::NetworkType::kWiFi, result->type);
        EXPECT_EQ(mojom::ConnectionStateType::kConnected,
                  result->connection_state);
        ASSERT_TRUE(result->wifi);
        EXPECT_EQ(mojom::SecurityType::kNone, result->wifi->security);
        EXPECT_EQ(50, result->wifi->signal_strength);
      }));
  cros_network_config()->GetNetworkState(
      "wifi2_guid", base::BindOnce([](mojom::NetworkStatePropertiesPtr result) {
        ASSERT_TRUE(result);
        EXPECT_EQ("wifi2_guid", result->guid);
        EXPECT_EQ(mojom::NetworkType::kWiFi, result->type);
        EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
                  result->connection_state);
        ASSERT_TRUE(result->wifi);
        EXPECT_EQ(mojom::SecurityType::kWpaPsk, result->wifi->security);
        EXPECT_EQ(100, result->wifi->signal_strength);
      }));
  cros_network_config()->GetNetworkState(
      "cellular_guid",
      base::BindOnce([](mojom::NetworkStatePropertiesPtr result) {
        ASSERT_TRUE(result);
        EXPECT_EQ("cellular_guid", result->guid);
        EXPECT_EQ(mojom::NetworkType::kCellular, result->type);
        EXPECT_EQ(mojom::ConnectionStateType::kNotConnected,
                  result->connection_state);
        ASSERT_TRUE(result->cellular);
        EXPECT_EQ(0, result->cellular->signal_strength);
        EXPECT_EQ("LTE", result->cellular->network_technology);
        EXPECT_EQ(mojom::ActivationStateType::kActivated,
                  result->cellular->activation_state);
      }));
  cros_network_config()->GetNetworkState(
      "vpn_guid", base::BindOnce([](mojom::NetworkStatePropertiesPtr result) {
        ASSERT_TRUE(result);
        EXPECT_EQ("vpn_guid", result->guid);
        EXPECT_EQ(mojom::NetworkType::kVPN, result->type);
        EXPECT_EQ(mojom::ConnectionStateType::kConnecting,
                  result->connection_state);
        ASSERT_TRUE(result->vpn);
        EXPECT_EQ(mojom::VPNType::kL2TPIPsec, result->vpn->type);
      }));
  // TODO(919691): Test ProxyMode once UIProxyConfigService logic is improved.
}

TEST_F(CrosNetworkConfigTest, GetNetworkStateList) {
  mojom::NetworkFilterPtr filter = mojom::NetworkFilter::New();
  // All active networks
  filter->filter = mojom::FilterType::kActive;
  filter->network_type = mojom::NetworkType::kAll;
  filter->limit = mojom::kNoLimit;
  cros_network_config()->GetNetworkStateList(
      filter.Clone(),
      base::BindOnce(
          [](std::vector<mojom::NetworkStatePropertiesPtr> networks) {
            ASSERT_EQ(3u, networks.size());
            EXPECT_EQ("eth_guid", networks[0]->guid);
            EXPECT_EQ("wifi1_guid", networks[1]->guid);
            EXPECT_EQ("vpn_guid", networks[2]->guid);
          }));

  // First active network
  filter->limit = 1;
  cros_network_config()->GetNetworkStateList(
      filter.Clone(),
      base::BindOnce(
          [](std::vector<mojom::NetworkStatePropertiesPtr> networks) {
            ASSERT_EQ(1u, networks.size());
            EXPECT_EQ("eth_guid", networks[0]->guid);
          }));

  // All wifi networks
  filter->filter = mojom::FilterType::kAll;
  filter->network_type = mojom::NetworkType::kWiFi;
  filter->limit = mojom::kNoLimit;
  cros_network_config()->GetNetworkStateList(
      filter.Clone(),
      base::BindOnce(
          [](std::vector<mojom::NetworkStatePropertiesPtr> networks) {
            ASSERT_EQ(3u, networks.size());
            EXPECT_EQ("wifi1_guid", networks[0]->guid);
            EXPECT_EQ("wifi2_guid", networks[1]->guid);
            EXPECT_EQ("wifi3_guid", networks[2]->guid);
          }));

  // Visible wifi networks
  filter->filter = mojom::FilterType::kVisible;
  cros_network_config()->GetNetworkStateList(
      filter.Clone(),
      base::BindOnce(
          [](std::vector<mojom::NetworkStatePropertiesPtr> networks) {
            ASSERT_EQ(2u, networks.size());
            EXPECT_EQ("wifi1_guid", networks[0]->guid);
            EXPECT_EQ("wifi2_guid", networks[1]->guid);
          }));

  // Configured wifi networks
  filter->filter = mojom::FilterType::kConfigured;
  cros_network_config()->GetNetworkStateList(
      filter.Clone(),
      base::BindOnce(
          [](std::vector<mojom::NetworkStatePropertiesPtr> networks) {
            ASSERT_EQ(1u, networks.size());
            EXPECT_EQ("wifi3_guid", networks[0]->guid);
          }));
}

TEST_F(CrosNetworkConfigTest, GetDeviceStateList) {
  cros_network_config()->GetDeviceStateList(
      base::BindOnce([](std::vector<mojom::DeviceStatePropertiesPtr> devices) {
        ASSERT_EQ(3u, devices.size());
        EXPECT_EQ(mojom::NetworkType::kWiFi, devices[0]->type);
        EXPECT_EQ(mojom::DeviceStateType::kEnabled, devices[0]->device_state);

        // IP address match those set up in FakeShillManagerClient::
        // SetupDefaultEnvironment(). TODO(stevenjb): Support setting
        // expectations explicitly in NetworkStateTestHelper.
        net::IPAddress ipv4_expected;
        ASSERT_TRUE(ipv4_expected.AssignFromIPLiteral("100.0.0.1"));
        EXPECT_EQ(ipv4_expected, devices[0]->ipv4_address);
        net::IPAddress ipv6_expected;
        ASSERT_TRUE(ipv6_expected.AssignFromIPLiteral("0:0:0:0:100:0:0:1"));
        EXPECT_EQ(ipv6_expected, devices[0]->ipv6_address);

        EXPECT_EQ(mojom::NetworkType::kEthernet, devices[1]->type);
        EXPECT_EQ(mojom::DeviceStateType::kEnabled, devices[1]->device_state);

        mojom::DeviceStateProperties* cellular = devices[2].get();
        EXPECT_EQ(mojom::NetworkType::kCellular, cellular->type);
        EXPECT_EQ(mojom::DeviceStateType::kEnabled, cellular->device_state);
        EXPECT_FALSE(cellular->sim_absent);
        ASSERT_TRUE(cellular->sim_lock_status);
        EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
        EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
        EXPECT_EQ(3, cellular->sim_lock_status->retries_left);
      }));

  // Disable WiFi
  helper().network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  cros_network_config()->GetDeviceStateList(
      base::BindOnce([](std::vector<mojom::DeviceStatePropertiesPtr> devices) {
        ASSERT_EQ(3u, devices.size());
        EXPECT_EQ(mojom::NetworkType::kWiFi, devices[0]->type);
        EXPECT_EQ(mojom::DeviceStateType::kDisabled, devices[0]->device_state);
      }));
}

TEST_F(CrosNetworkConfigTest, SetNetworkTypeEnabledState) {
  cros_network_config()->GetDeviceStateList(
      base::BindOnce([](std::vector<mojom::DeviceStatePropertiesPtr> devices) {
        ASSERT_EQ(3u, devices.size());
        EXPECT_EQ(mojom::NetworkType::kWiFi, devices[0]->type);
        EXPECT_EQ(mojom::DeviceStateType::kEnabled, devices[0]->device_state);
      }));
  // Disable WiFi
  bool succeeded = false;
  cros_network_config()->SetNetworkTypeEnabledState(
      mojom::NetworkType::kWiFi, false,
      base::BindOnce(
          [](bool* succeeded, bool success) { *succeeded = success; },
          &succeeded));
  // Wait for callback to complete; this test does not use mojo bindings.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(succeeded);
  cros_network_config()->GetDeviceStateList(
      base::BindOnce([](std::vector<mojom::DeviceStatePropertiesPtr> devices) {
        ASSERT_EQ(3u, devices.size());
        EXPECT_EQ(mojom::NetworkType::kWiFi, devices[0]->type);
        EXPECT_EQ(mojom::DeviceStateType::kDisabled, devices[0]->device_state);
      }));
}

TEST_F(CrosNetworkConfigTest, SetCellularSimState) {
  // Assert initial state.
  mojom::DeviceStatePropertiesPtr cellular =
      GetDeviceState(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular);
  ASSERT_FALSE(cellular->sim_absent);
  ASSERT_TRUE(cellular->sim_lock_status);
  ASSERT_TRUE(cellular->sim_lock_status->lock_enabled);
  ASSERT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
  const int retries = FakeShillDeviceClient::kSimPinRetryCount;
  ASSERT_EQ(retries, cellular->sim_lock_status->retries_left);

  // Unlock the sim with the correct pin. |require_pin| should be ignored.
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                  /*new_pin=*/base::nullopt,
                                  /*require_pin=*/false));

  // Sim should be unlocked, locking should still be enabled.
  cellular = GetDeviceState(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Set |require_pin| to false (disable locking).
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                  /*new_pin=*/base::nullopt,
                                  /*require_pin=*/false));

  // Sim should be unlocked, locking should be disabled.
  cellular = GetDeviceState(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_FALSE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Set |require_pin| to true (enable locking).
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kDefaultSimPin,
                                  /*new_pin=*/base::nullopt,
                                  /*require_pin=*/true));

  // Sim should remain unlocked, locking should be enabled.
  cellular = GetDeviceState(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());

  // Lock the sim. (Can not be done via the mojo API).
  helper().device_test()->SetSimLocked(kCellularDevicePath, true);
  base::RunLoop().RunUntilIdle();
  cellular = GetDeviceState(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  ASSERT_TRUE(cellular->sim_lock_status->lock_enabled);
  ASSERT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);

  // Attempt to unlock the sim with an incorrect pin. Call should fail.
  EXPECT_FALSE(SetCellularSimState("incorrect pin", /*new_pin=*/base::nullopt,
                                   /*require_pin=*/false));

  // Ensure sim is still locked and retry count has decreased.
  cellular = GetDeviceState(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPin, cellular->sim_lock_status->lock_type);
  EXPECT_EQ(retries - 1, cellular->sim_lock_status->retries_left);

  // Additional attempts should set the sim to puk locked.
  for (int i = retries - 1; i > 0; --i) {
    SetCellularSimState("incorrect pin", /*new_pin=*/base::nullopt, false);
  }
  cellular = GetDeviceState(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_enabled);
  EXPECT_EQ(shill::kSIMLockPuk, cellular->sim_lock_status->lock_type);

  // Attempt to unblock the sim with the incorrect puk. Call should fail.
  const std::string new_pin = "2222";
  EXPECT_FALSE(SetCellularSimState("incorrect puk",
                                   base::make_optional(new_pin),
                                   /*require_pin=*/false));

  // Attempt to unblock the sim with np pin. Call should fail.
  EXPECT_FALSE(SetCellularSimState(FakeShillDeviceClient::kSimPuk,
                                   /*new_pin=*/base::nullopt,
                                   /*require_pin=*/false));

  // Attempt to unlock the sim with the correct puk.
  EXPECT_TRUE(SetCellularSimState(FakeShillDeviceClient::kSimPuk,
                                  base::make_optional(new_pin),
                                  /*require_pin=*/false));

  // Sim should be unlocked
  cellular = GetDeviceState(mojom::NetworkType::kCellular);
  ASSERT_TRUE(cellular && cellular->sim_lock_status);
  EXPECT_TRUE(cellular->sim_lock_status->lock_type.empty());
}

TEST_F(CrosNetworkConfigTest, RequestNetworkScan) {
  // Observe device state list changes and track when the wifi scanning state
  // gets set to true. Note: In the test the scan will complete immediately and
  // the scanning state will get set back to false, so ignore that change.
  class ScanningObserver : public CrosNetworkConfigTestObserver {
   public:
    explicit ScanningObserver(CrosNetworkConfig* cros_network_config)
        : cros_network_config_(cros_network_config) {}
    void OnDeviceStateListChanged() override {
      cros_network_config_->GetDeviceStateList(base::BindOnce(
          [](bool* wifi_scanning,
             std::vector<mojom::DeviceStatePropertiesPtr> devices) {
            for (auto& device : devices) {
              if (device->type == mojom::NetworkType::kWiFi && device->scanning)
                *wifi_scanning = true;
            }
          },
          &wifi_scanning_));
    }
    CrosNetworkConfig* cros_network_config_;
    bool wifi_scanning_ = false;
  };
  ScanningObserver observer(cros_network_config());
  cros_network_config()->AddObserver(observer.GenerateInterfacePtr());
  base::RunLoop().RunUntilIdle();

  cros_network_config()->RequestNetworkScan(mojom::NetworkType::kWiFi);
  base::RunLoop().RunUntilIdle();
  observer.FlushForTesting();
  EXPECT_TRUE(observer.wifi_scanning_);
}

TEST_F(CrosNetworkConfigTest, NetworkListChanged) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer()->network_state_list_changed());

  // Add a wifi network.
  helper().ConfigureService(
      R"({"GUID": "wifi3_guid", "Type": "wifi", "State": "ready"})");
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer()->network_state_list_changed());
}

TEST_F(CrosNetworkConfigTest, DeviceListChanged) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer()->device_state_list_changed());

  // Disable wifi
  helper().network_state_handler()->SetTechnologyEnabled(
      NetworkTypePattern::WiFi(), false, network_handler::ErrorCallback());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer()->device_state_list_changed());
}

TEST_F(CrosNetworkConfigTest, ActiveNetworksChanged) {
  SetupObserver();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0, observer()->active_networks_changed());

  // Change a network state.
  helper().SetServiceProperty(wifi1_path(), shill::kStateProperty,
                              base::Value(shill::kStateIdle));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, observer()->active_networks_changed());
}

}  // namespace network_config
}  // namespace chromeos
