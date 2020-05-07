// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/network_config_service.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sync_wifi/local_network_collector.h"
#include "chromeos/components/sync_wifi/local_network_collector_impl.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/components/sync_wifi/network_test_helper.h"
#include "chromeos/components/sync_wifi/network_type_conversions.h"
#include "chromeos/components/sync_wifi/test_data_generator.h"
#include "chromeos/dbus/shill/fake_shill_simulated_result.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_metadata_store.h"
#include "chromeos/services/network_config/cros_network_config.h"
#include "chromeos/services/network_config/in_process_instance.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class NetworkDeviceHandler;
class NetworkProfileHandler;
class NetworkConfigurationHandler;
class NetworkConnectionHandler;
class ManagedNetworkConfigurationHandler;

namespace sync_wifi {

namespace {

const char kFredSsid[] = "Fred";
const char kMangoSsid[] = "Mango";
const char kAnnieSsid[] = "Annie";
const char kOzzySsid[] = "Ozzy";
const char kHopperSsid[] = "Hopper";
const char kByteSsid[] = "Byte";

}  // namespace

class LocalNetworkCollectorImplTest : public testing::Test {
 public:
  LocalNetworkCollectorImplTest() {
    local_test_helper_ = std::make_unique<NetworkTestHelper>();
    ash::GetNetworkConfigService(
        remote_cros_network_config_.BindNewPipeAndPassReceiver());
  }
  ~LocalNetworkCollectorImplTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();
    helper()->SetUp();
    local_network_collector_ = std::make_unique<LocalNetworkCollectorImpl>(
        remote_cros_network_config_.get());
    local_network_collector_->SetNetworkMetadataStore(
        NetworkHandler::Get()->network_metadata_store()->GetWeakPtr());
  }

  void TearDown() override {
    chromeos::NetworkHandler::Shutdown();
    testing::Test::TearDown();
  }

  void OnGetAllSyncableNetworks(
      std::vector<std::string> expected,
      std::vector<sync_pb::WifiConfigurationSpecifics> result) {
    EXPECT_EQ(expected.size(), result.size());
    for (int i = 0; i < (int)result.size(); i++) {
      EXPECT_EQ(expected[i], DecodeHexString(result[i].hex_ssid()));
    }
  }

  void OnGetSyncableNetwork(
      std::string expected_ssid,
      base::Optional<sync_pb::WifiConfigurationSpecifics> result) {
    if (expected_ssid.empty()) {
      ASSERT_EQ(base::nullopt, result);
      return;
    }
    EXPECT_EQ(expected_ssid, DecodeHexString(result->hex_ssid()));
    EXPECT_TRUE(result->has_last_connected_timestamp());
    EXPECT_NE(0, result->last_connected_timestamp());
  }

  LocalNetworkCollector* local_network_collector() {
    return local_network_collector_.get();
  }

  void TestGetSyncableNetwork(const std::string& guid,
                              const std::string& expected_ssid) {
    local_network_collector()->GetSyncableNetwork(
        guid,
        base::BindOnce(&LocalNetworkCollectorImplTest::OnGetSyncableNetwork,
                       base::Unretained(this), expected_ssid));
    base::RunLoop().RunUntilIdle();
  }

  NetworkTestHelper* helper() { return local_test_helper_.get(); }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<LocalNetworkCollector> local_network_collector_;
  std::unique_ptr<NetworkTestHelper> local_test_helper_;
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  DISALLOW_COPY_AND_ASSIGN(LocalNetworkCollectorImplTest);
};

TEST_F(LocalNetworkCollectorImplTest, TestGetAllSyncableNetworks) {
  helper()->ConfigureWiFiNetwork(kFredSsid, /*is_secured=*/true,
                                 /*in_profile=*/true, /*has_connected=*/true);

  std::vector<std::string> expected;
  expected.push_back(kFredSsid);

  local_network_collector()->GetAllSyncableNetworks(
      base::BindOnce(&LocalNetworkCollectorImplTest::OnGetAllSyncableNetworks,
                     base::Unretained(this), expected));

  base::RunLoop().RunUntilIdle();
}

TEST_F(LocalNetworkCollectorImplTest,
       TestGetAllSyncableNetworks_WithFiltering) {
  helper()->ConfigureWiFiNetwork(kFredSsid, /*is_secured=*/true,
                                 /*in_profile=*/true, /*has_connected=*/true);
  helper()->ConfigureWiFiNetwork(kMangoSsid, /*is_secured=*/true,
                                 /*in_profile=*/false, /*has_connected=*/true,
                                 /*owned_by_user=*/false);
  helper()->ConfigureWiFiNetwork(kAnnieSsid, /*is_secured=*/false,
                                 /*in_profile=*/true, /*has_connected=*/true);
  helper()->ConfigureWiFiNetwork(kOzzySsid, /*is_secured=*/true,
                                 /*in_profile=*/true, /*has_connected=*/true);
  helper()->ConfigureWiFiNetwork(kHopperSsid, /*is_secured=*/true,
                                 /*in_profile=*/true, /*has_connected=*/false);
  helper()->ConfigureWiFiNetwork(kByteSsid, /*is_secured=*/true,
                                 /*in_profile=*/false, /*has_connected=*/true,
                                 /*owned_by_user=*/true);

  std::vector<std::string> expected;
  expected.push_back(kByteSsid);
  expected.push_back(kFredSsid);
  expected.push_back(kOzzySsid);

  local_network_collector()->GetAllSyncableNetworks(
      base::BindOnce(&LocalNetworkCollectorImplTest::OnGetAllSyncableNetworks,
                     base::Unretained(this), expected));

  base::RunLoop().RunUntilIdle();
}

TEST_F(LocalNetworkCollectorImplTest, TestGetSyncableNetwork) {
  std::string guid = helper()->ConfigureWiFiNetwork(
      kFredSsid, /*is_secured=*/true,
      /*in_profile=*/true, /*has_connected=*/true);
  TestGetSyncableNetwork(guid, kFredSsid);
}

TEST_F(LocalNetworkCollectorImplTest,
       TestGetSyncableNetwork_Shared_OwnedByUser) {
  std::string guid = helper()->ConfigureWiFiNetwork(
      kFredSsid, /*is_secured=*/true,
      /*in_profile=*/false, /*has_connected=*/true, /*owned_by_user=*/true);
  TestGetSyncableNetwork(guid, kFredSsid);
}

TEST_F(LocalNetworkCollectorImplTest,
       TestGetSyncableNetwork_Shared_OwnedByOther) {
  std::string guid = helper()->ConfigureWiFiNetwork(
      kFredSsid, /*is_secured=*/true,
      /*in_profile=*/false, /*has_connected=*/true, /*owned_by_user=*/false);
  TestGetSyncableNetwork(guid, /*expected_ssid=*/std::string());
}

TEST_F(LocalNetworkCollectorImplTest, TestGetSyncableNetwork_DoesntExist) {
  TestGetSyncableNetwork("test_guid", /*expected_ssid=*/std::string());
}

TEST_F(LocalNetworkCollectorImplTest, TestGetSyncableNetwork_NeverConnected) {
  std::string guid = helper()->ConfigureWiFiNetwork(
      kFredSsid, /*is_secured=*/true,
      /*in_profile=*/true, /*has_connected=*/false);
  TestGetSyncableNetwork(guid, /*expected_ssid=*/std::string());
}

}  // namespace sync_wifi

}  // namespace chromeos
