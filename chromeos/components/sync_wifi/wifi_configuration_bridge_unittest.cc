// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/wifi_configuration_bridge.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "chromeos/components/sync_wifi/synced_network_updater.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_wifi {

namespace {

using sync_pb::WifiConfigurationSpecificsData;
using testing::_;
using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Pair;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;

const char kSsidMeow[] = "meow";
const char kSsidWoof[] = "woof";

WifiConfigurationSpecificsData CreateSpecifics(const std::string& ssid) {
  WifiConfigurationSpecificsData specifics;
  specifics.set_ssid(ssid);
  specifics.set_security_type(
      WifiConfigurationSpecificsData::SECURITY_TYPE_PSK);
  specifics.set_passphrase("password");
  specifics.set_automatically_connect(
      WifiConfigurationSpecificsData::AUTOMATICALLY_CONNECT_ENABLED);
  specifics.set_is_preferred(
      WifiConfigurationSpecificsData::IS_PREFERRED_ENABLED);
  specifics.set_metered(WifiConfigurationSpecificsData::METERED_OPTION_AUTO);
  sync_pb::WifiConfigurationSpecificsData_ProxyConfiguration proxy_config;
  proxy_config.set_proxy_option(WifiConfigurationSpecificsData::
                                    ProxyConfiguration::PROXY_OPTION_DISABLED);
  specifics.mutable_proxy_configuration()->CopyFrom(proxy_config);
  return specifics;
}

std::unique_ptr<syncer::EntityData> GenerateWifiEntityData(
    const sync_pb::WifiConfigurationSpecificsData& data) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_wifi_configuration()
      ->mutable_client_only_encrypted_data()
      ->CopyFrom(data);
  entity_data->name = data.ssid();
  return entity_data;
}

bool VectorContainsString(std::vector<std::string> v, std::string s) {
  return std::find(v.begin(), v.end(), s) != v.end();
}

bool VectorContainsSsid(
    const std::vector<sync_pb::WifiConfigurationSpecificsData>& v,
    std::string s) {
  for (sync_pb::WifiConfigurationSpecificsData specifics : v) {
    if (specifics.ssid() == s)
      return true;
  }
  return false;
}

class TestSyncedNetworkUpdater : public SyncedNetworkUpdater {
 public:
  TestSyncedNetworkUpdater() = default;
  ~TestSyncedNetworkUpdater() override = default;

  const std::vector<sync_pb::WifiConfigurationSpecificsData>&
  add_or_update_calls() {
    return add_update_calls_;
  }

  const std::vector<std::string>& remove_calls() { return remove_calls_; }

 private:
  void AddOrUpdateNetwork(
      const sync_pb::WifiConfigurationSpecificsData& specifics) override {
    add_update_calls_.push_back(specifics);
  }

  void RemoveNetwork(const std::string& ssid) override {
    remove_calls_.push_back(ssid);
  }

  std::vector<sync_pb::WifiConfigurationSpecificsData> add_update_calls_;
  std::vector<std::string> remove_calls_;
};

class WifiConfigurationBridgeTest : public testing::Test {
 protected:
  WifiConfigurationBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void SetUp() override {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    synced_network_updater_ = std::make_unique<TestSyncedNetworkUpdater>();
    bridge_ = std::make_unique<WifiConfigurationBridge>(
        synced_network_updater(), mock_processor_.CreateForwardingProcessor(),
        syncer::ModelTypeStoreTestUtil::MoveStoreToFactory(std::move(store_)));
  }

  void DisableBridge() {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(false));
  }

  syncer::EntityChangeList CreateEntityAddList(
      const std::vector<WifiConfigurationSpecificsData>& specifics_list) {
    syncer::EntityChangeList changes;
    for (const auto& data : specifics_list) {
      auto entity_data = std::make_unique<syncer::EntityData>();
      sync_pb::WifiConfigurationSpecifics specifics;

      specifics.mutable_client_only_encrypted_data()->CopyFrom(data);
      entity_data->specifics.mutable_wifi_configuration()->CopyFrom(specifics);

      entity_data->name = data.ssid();

      changes.push_back(
          syncer::EntityChange::CreateAdd(data.ssid(), std::move(entity_data)));
    }
    return changes;
  }

  syncer::MockModelTypeChangeProcessor* processor() { return &mock_processor_; }

  WifiConfigurationBridge* bridge() { return bridge_.get(); }

  TestSyncedNetworkUpdater* synced_network_updater() {
    return synced_network_updater_.get();
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_;

  std::unique_ptr<syncer::ModelTypeStore> store_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;

  std::unique_ptr<WifiConfigurationBridge> bridge_;

  std::unique_ptr<TestSyncedNetworkUpdater> synced_network_updater_;

  DISALLOW_COPY_AND_ASSIGN(WifiConfigurationBridgeTest);
};

TEST_F(WifiConfigurationBridgeTest, InitWithTwoNetworksFromServer) {
  syncer::EntityChangeList remote_input;

  WifiConfigurationSpecificsData entry1 = CreateSpecifics(kSsidMeow);
  WifiConfigurationSpecificsData entry2 = CreateSpecifics(kSsidWoof);

  remote_input.push_back(syncer::EntityChange::CreateAdd(
      kSsidMeow, GenerateWifiEntityData(entry1)));
  remote_input.push_back(syncer::EntityChange::CreateAdd(
      kSsidWoof, GenerateWifiEntityData(entry2)));

  bridge()->MergeSyncData(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(remote_input));

  std::vector<std::string> ssids = bridge()->GetAllSsidsForTesting();
  EXPECT_EQ(2u, ssids.size());
  EXPECT_TRUE(VectorContainsString(ssids, kSsidMeow));
  EXPECT_TRUE(VectorContainsString(ssids, kSsidWoof));

  const std::vector<sync_pb::WifiConfigurationSpecificsData>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(2u, networks.size());
  EXPECT_TRUE(VectorContainsSsid(networks, kSsidMeow));
  EXPECT_TRUE(VectorContainsSsid(networks, kSsidWoof));
}

TEST_F(WifiConfigurationBridgeTest, ApplySyncChangesAddTwoSpecifics) {
  const WifiConfigurationSpecificsData specifics1 = CreateSpecifics(kSsidMeow);
  const WifiConfigurationSpecificsData specifics2 = CreateSpecifics(kSsidWoof);

  base::Optional<syncer::ModelError> error =
      bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                 CreateEntityAddList({specifics1, specifics2}));
  EXPECT_FALSE(error);
  std::vector<std::string> ssids = bridge()->GetAllSsidsForTesting();
  EXPECT_EQ(2u, ssids.size());
  EXPECT_TRUE(VectorContainsString(ssids, kSsidMeow));
  EXPECT_TRUE(VectorContainsString(ssids, kSsidWoof));

  const std::vector<sync_pb::WifiConfigurationSpecificsData>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(2u, networks.size());
  EXPECT_TRUE(VectorContainsSsid(networks, kSsidMeow));
  EXPECT_TRUE(VectorContainsSsid(networks, kSsidWoof));
}

TEST_F(WifiConfigurationBridgeTest, ApplySyncChangesOneAdd) {
  WifiConfigurationSpecificsData entry = CreateSpecifics(kSsidMeow);

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kSsidMeow, GenerateWifiEntityData(entry)));

  bridge()->ApplySyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(add_changes));
  std::vector<std::string> ssids = bridge()->GetAllSsidsForTesting();
  EXPECT_EQ(1u, ssids.size());
  EXPECT_TRUE(VectorContainsString(ssids, kSsidMeow));

  const std::vector<sync_pb::WifiConfigurationSpecificsData>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, networks.size());
  EXPECT_TRUE(VectorContainsSsid(networks, kSsidMeow));
}

TEST_F(WifiConfigurationBridgeTest, ApplySyncChangesOneDeletion) {
  WifiConfigurationSpecificsData entry = CreateSpecifics(kSsidMeow);

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      kSsidMeow, GenerateWifiEntityData(entry)));

  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             std::move(add_changes));
  std::vector<std::string> ssids = bridge()->GetAllSsidsForTesting();
  EXPECT_EQ(1u, ssids.size());
  EXPECT_TRUE(VectorContainsString(ssids, kSsidMeow));

  const std::vector<sync_pb::WifiConfigurationSpecificsData>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, networks.size());
  EXPECT_TRUE(VectorContainsSsid(networks, kSsidMeow));

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(syncer::EntityChange::CreateDelete(kSsidMeow));

  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             std::move(delete_changes));
  EXPECT_TRUE(bridge()->GetAllSsidsForTesting().empty());

  const std::vector<std::string>& removed_networks =
      synced_network_updater()->remove_calls();
  EXPECT_EQ(1u, removed_networks.size());
  EXPECT_TRUE(VectorContainsString(removed_networks, kSsidMeow));
}

}  // namespace

}  // namespace sync_wifi