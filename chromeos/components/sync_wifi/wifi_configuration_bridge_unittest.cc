// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/sync_wifi/wifi_configuration_bridge.h"

#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "chromeos/components/sync_wifi/fake_local_network_collector.h"
#include "chromeos/components/sync_wifi/network_identifier.h"
#include "chromeos/components/sync_wifi/synced_network_updater.h"
#include "chromeos/components/sync_wifi/test_data_generator.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/model_impl/in_memory_metadata_change_list.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/test_matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace sync_wifi {

namespace {

using sync_pb::WifiConfigurationSpecifics;
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

syncer::EntityData GenerateWifiEntityData(
    const sync_pb::WifiConfigurationSpecifics& data) {
  syncer::EntityData entity_data;
  entity_data.specifics.mutable_wifi_configuration()
      ->CopyFrom(data);
  entity_data.name = data.hex_ssid();
  return entity_data;
}

bool ProtoVectorContainsId(
    const std::vector<sync_pb::WifiConfigurationSpecifics>& protos,
    NetworkIdentifier id) {
  return std::find_if(
             protos.begin(), protos.end(),
             [&id](const sync_pb::WifiConfigurationSpecifics& specifics) {
               return NetworkIdentifier::FromProto(specifics) == id;
             }) != protos.end();
}

void ExtractProtosFromDataBatch(
    std::unique_ptr<syncer::DataBatch> batch,
    std::vector<sync_pb::WifiConfigurationSpecifics>* output) {
  while (batch->HasNext()) {
    const syncer::KeyAndData& data_pair = batch->Next();
    output->push_back(data_pair.second->specifics.wifi_configuration());
  }
}

// Implementation of SyncedNetworkUpdater. This class takes add/update/delete
// network requests and stores them in its internal data structures without
// actually updating anything external.
class TestSyncedNetworkUpdater : public SyncedNetworkUpdater {
 public:
  TestSyncedNetworkUpdater() = default;
  ~TestSyncedNetworkUpdater() override = default;

  const std::vector<sync_pb::WifiConfigurationSpecifics>&
  add_or_update_calls() {
    return add_update_calls_;
  }

  const std::vector<NetworkIdentifier>& remove_calls() { return remove_calls_; }

 private:
  void AddOrUpdateNetwork(
      const sync_pb::WifiConfigurationSpecifics& specifics) override {
    add_update_calls_.push_back(specifics);
  }

  void RemoveNetwork(const NetworkIdentifier& id) override {
    remove_calls_.push_back(id);
  }

  std::vector<sync_pb::WifiConfigurationSpecifics> add_update_calls_;
  std::vector<NetworkIdentifier> remove_calls_;
};

class WifiConfigurationBridgeTest : public testing::Test {
 protected:
  WifiConfigurationBridgeTest()
      : store_(syncer::ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {}

  void SetUp() override {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(true));
    synced_network_updater_ = std::make_unique<TestSyncedNetworkUpdater>();
    local_network_collector_ = std::make_unique<FakeLocalNetworkCollector>();
    bridge_ = std::make_unique<WifiConfigurationBridge>(
        synced_network_updater(), local_network_collector(),
        mock_processor_.CreateForwardingProcessor(),
        syncer::ModelTypeStoreTestUtil::MoveStoreToFactory(std::move(store_)));
  }

  void DisableBridge() {
    ON_CALL(mock_processor_, IsTrackingMetadata()).WillByDefault(Return(false));
  }

  syncer::EntityChangeList CreateEntityAddList(
      const std::vector<WifiConfigurationSpecifics>& specifics_list) {
    syncer::EntityChangeList changes;
    for (const auto& proto : specifics_list) {
      syncer::EntityData entity_data;
      entity_data.specifics.mutable_wifi_configuration()->CopyFrom(proto);
      entity_data.name = proto.hex_ssid();

      changes.push_back(syncer::EntityChange::CreateAdd(
          proto.hex_ssid(), std::move(entity_data)));
    }
    return changes;
  }

  std::vector<sync_pb::WifiConfigurationSpecifics> GetAllSyncedData() {
    std::vector<WifiConfigurationSpecifics> data;
    base::RunLoop loop;
    bridge()->GetAllDataForDebugging(base::BindLambdaForTesting(
        [&loop, &data](std::unique_ptr<syncer::DataBatch> batch) {
          ExtractProtosFromDataBatch(std::move(batch), &data);
          loop.Quit();
        }));
    loop.Run();
    return data;
  }

  syncer::MockModelTypeChangeProcessor* processor() { return &mock_processor_; }

  WifiConfigurationBridge* bridge() { return bridge_.get(); }

  TestSyncedNetworkUpdater* synced_network_updater() {
    return synced_network_updater_.get();
  }

  FakeLocalNetworkCollector* local_network_collector() {
    return local_network_collector_.get();
  }

  const NetworkIdentifier& woof_network_id() const { return woof_network_id_; }

  const NetworkIdentifier& meow_network_id() const { return meow_network_id_; }

 private:
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<syncer::ModelTypeStore> store_;

  testing::NiceMock<syncer::MockModelTypeChangeProcessor> mock_processor_;

  std::unique_ptr<WifiConfigurationBridge> bridge_;

  std::unique_ptr<TestSyncedNetworkUpdater> synced_network_updater_;

  std::unique_ptr<FakeLocalNetworkCollector> local_network_collector_;

  const NetworkIdentifier woof_network_id_ = GeneratePskNetworkId(kSsidWoof);

  const NetworkIdentifier meow_network_id_ = GeneratePskNetworkId(kSsidMeow);

  DISALLOW_COPY_AND_ASSIGN(WifiConfigurationBridgeTest);
};

TEST_F(WifiConfigurationBridgeTest, InitWithTwoNetworksFromServer) {
  syncer::EntityChangeList remote_input;

  WifiConfigurationSpecifics entry1 =
      GenerateTestWifiSpecifics(meow_network_id());
  WifiConfigurationSpecifics entry2 =
      GenerateTestWifiSpecifics(woof_network_id());

  remote_input.push_back(syncer::EntityChange::CreateAdd(
      meow_network_id().SerializeToString(), GenerateWifiEntityData(entry1)));
  remote_input.push_back(syncer::EntityChange::CreateAdd(
      woof_network_id().SerializeToString(), GenerateWifiEntityData(entry2)));

  bridge()->MergeSyncData(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(remote_input));

  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(2u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));
  EXPECT_TRUE(base::Contains(ids, woof_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecifics>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(2u, networks.size());
  EXPECT_TRUE(ProtoVectorContainsId(networks, meow_network_id()));
  EXPECT_TRUE(ProtoVectorContainsId(networks, woof_network_id()));
}

TEST_F(WifiConfigurationBridgeTest, ApplySyncChangesAddTwoSpecifics) {
  const WifiConfigurationSpecifics specifics1 =
      GenerateTestWifiSpecifics(meow_network_id());
  const WifiConfigurationSpecifics specifics2 =
      GenerateTestWifiSpecifics(woof_network_id());

  base::Optional<syncer::ModelError> error =
      bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                 CreateEntityAddList({specifics1, specifics2}));
  EXPECT_FALSE(error);
  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(2u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));
  EXPECT_TRUE(base::Contains(ids, woof_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecifics>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(2u, networks.size());
  EXPECT_TRUE(ProtoVectorContainsId(networks, meow_network_id()));
  EXPECT_TRUE(ProtoVectorContainsId(networks, woof_network_id()));
}

TEST_F(WifiConfigurationBridgeTest, ApplySyncChangesOneAdd) {
  WifiConfigurationSpecifics entry =
      GenerateTestWifiSpecifics(meow_network_id());

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      meow_network_id().SerializeToString(), GenerateWifiEntityData(entry)));

  bridge()->ApplySyncChanges(
      std::make_unique<syncer::InMemoryMetadataChangeList>(),
      std::move(add_changes));
  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(1u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecifics>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, networks.size());
  EXPECT_TRUE(ProtoVectorContainsId(networks, meow_network_id()));
}

TEST_F(WifiConfigurationBridgeTest, ApplySyncChangesOneDeletion) {
  WifiConfigurationSpecifics entry =
      GenerateTestWifiSpecifics(meow_network_id());
  NetworkIdentifier id = NetworkIdentifier::FromProto(entry);

  syncer::EntityChangeList add_changes;

  add_changes.push_back(syncer::EntityChange::CreateAdd(
      id.SerializeToString(), GenerateWifiEntityData(entry)));

  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             std::move(add_changes));
  std::vector<NetworkIdentifier> ids = bridge()->GetAllIdsForTesting();
  EXPECT_EQ(1u, ids.size());
  EXPECT_TRUE(base::Contains(ids, meow_network_id()));

  const std::vector<sync_pb::WifiConfigurationSpecifics>& networks =
      synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, networks.size());
  EXPECT_TRUE(ProtoVectorContainsId(networks, meow_network_id()));

  syncer::EntityChangeList delete_changes;
  delete_changes.push_back(
      syncer::EntityChange::CreateDelete(id.SerializeToString()));

  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             std::move(delete_changes));
  EXPECT_TRUE(bridge()->GetAllIdsForTesting().empty());

  const std::vector<NetworkIdentifier>& removed_networks =
      synced_network_updater()->remove_calls();
  EXPECT_EQ(1u, removed_networks.size());
  EXPECT_EQ(removed_networks[0], id);
}

TEST_F(WifiConfigurationBridgeTest, MergeSyncData) {
  auto metadata_change_list =
      std::make_unique<syncer::InMemoryMetadataChangeList>();
  syncer::EntityChangeList entity_data;

  WifiConfigurationSpecifics meow_network =
      GenerateTestWifiSpecifics(meow_network_id());
  entity_data.push_back(
      syncer::EntityChange::CreateAdd(meow_network_id().SerializeToString(),
                                      GenerateWifiEntityData(meow_network)));

  WifiConfigurationSpecifics woof_network =
      GenerateTestWifiSpecifics(woof_network_id());
  local_network_collector()->AddNetwork(woof_network);

  std::string storage_key;
  EXPECT_CALL(*processor(), Put(_, _, _))
      .WillOnce(testing::SaveArg<0>(&storage_key));

  bridge()->MergeSyncData(std::move(metadata_change_list),
                          std::move(entity_data));
  base::RunLoop().RunUntilIdle();

  // Verify local network was added to sync.
  EXPECT_EQ(storage_key, woof_network_id().SerializeToString());

  // Verify sync network was added to local stack.
  const std::vector<sync_pb::WifiConfigurationSpecifics>&
      updated_local_networks = synced_network_updater()->add_or_update_calls();
  EXPECT_EQ(1u, updated_local_networks.size());
  EXPECT_TRUE(ProtoVectorContainsId(updated_local_networks, meow_network_id()));

  std::vector<sync_pb::WifiConfigurationSpecifics> sync_networks =
      GetAllSyncedData();
  EXPECT_EQ(2u, sync_networks.size());
}

}  // namespace

}  // namespace sync_wifi

}  // namespace chromeos
