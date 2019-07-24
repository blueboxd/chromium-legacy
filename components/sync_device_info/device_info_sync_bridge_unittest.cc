// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/device_info_sync_bridge.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_task_environment.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/base/time.h"
#include "components/sync/model/data_batch.h"
#include "components/sync/model/data_type_activation_request.h"
#include "components/sync/model/data_type_error_handler_mock.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mock_model_type_change_processor.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/sync/test/test_matchers.h"
#include "components/sync_device_info/device_info_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

using base::OneShotTimer;
using sync_pb::DeviceInfoSpecifics;
using sync_pb::EntitySpecifics;
using sync_pb::ModelTypeState;
using testing::_;
using testing::IsEmpty;
using testing::Matcher;
using testing::NotNull;
using testing::Pair;
using testing::Return;
using testing::SizeIs;
using testing::UnorderedElementsAre;

using DeviceInfoList = std::vector<std::unique_ptr<DeviceInfo>>;
using StorageKeyList = ModelTypeSyncBridge::StorageKeyList;
using RecordList = ModelTypeStore::RecordList;
using StartCallback = ModelTypeControllerDelegate::StartCallback;
using WriteBatch = ModelTypeStore::WriteBatch;

const int kLocalSuffix = 0;

MATCHER_P(HasDeviceInfo, expected, "") {
  return arg.device_info().SerializeAsString() == expected.SerializeAsString();
}

MATCHER_P(EqualsProto, expected, "") {
  return arg.SerializeAsString() == expected.SerializeAsString();
}

MATCHER_P(ModelEqualsSpecifics, expected_specifics, "") {
  // Note that we ignore the device name here to avoid having to inject the
  // local device's.
  return expected_specifics.cache_guid() == arg.guid() &&
         expected_specifics.device_type() == arg.device_type() &&
         expected_specifics.sync_user_agent() == arg.sync_user_agent() &&
         expected_specifics.chrome_version() == arg.chrome_version() &&
         expected_specifics.signin_scoped_device_id() ==
             arg.signin_scoped_device_id() &&
         expected_specifics.feature_fields()
                 .send_tab_to_self_receiving_enabled() ==
             arg.send_tab_to_self_receiving_enabled();
}

Matcher<std::unique_ptr<EntityData>> HasSpecifics(
    const Matcher<sync_pb::EntitySpecifics>& m) {
  return testing::Pointee(testing::Field(&EntityData::specifics, m));
}

MATCHER(HasLastUpdatedAboutNow, "") {
  const sync_pb::DeviceInfoSpecifics& specifics = arg.device_info();
  const base::Time now = base::Time::Now();
  const base::TimeDelta tolerance = base::TimeDelta::FromMinutes(1);
  const base::Time actual_last_updated =
      ProtoTimeToTime(specifics.last_updated_timestamp());

  if (actual_last_updated < now - tolerance) {
    *result_listener << "which is too far in the past";
    return false;
  }
  if (actual_last_updated > now + tolerance) {
    *result_listener << "which is too far in the future";
    return false;
  }
  return true;
}

std::string CacheGuidForSuffix(int suffix) {
  return base::StringPrintf("cache guid %d", suffix);
}

std::string ClientNameForSuffix(int suffix) {
  return base::StringPrintf("client name %d", suffix);
}

std::string SyncUserAgentForSuffix(int suffix) {
  return base::StringPrintf("sync user agent %d", suffix);
}

std::string ChromeVersionForSuffix(int suffix) {
  return base::StringPrintf("chrome version %d", suffix);
}

std::string SigninScopedDeviceIdForSuffix(int suffix) {
  return base::StringPrintf("signin scoped device id %d", suffix);
}

DataTypeActivationRequest TestDataTypeActivationRequest() {
  DataTypeActivationRequest request;
  request.cache_guid = CacheGuidForSuffix(kLocalSuffix);
  return request;
}

DeviceInfoSpecifics CreateSpecifics(
    int suffix,
    base::Time last_updated = base::Time::Now()) {
  DeviceInfoSpecifics specifics;
  specifics.set_cache_guid(CacheGuidForSuffix(suffix));
  specifics.set_client_name(ClientNameForSuffix(suffix));
  specifics.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_LINUX);
  specifics.set_sync_user_agent(SyncUserAgentForSuffix(suffix));
  specifics.set_chrome_version(ChromeVersionForSuffix(suffix));
  specifics.set_signin_scoped_device_id(SigninScopedDeviceIdForSuffix(suffix));
  specifics.set_last_updated_timestamp(TimeToProtoTime(last_updated));
  specifics.mutable_feature_fields()->set_send_tab_to_self_receiving_enabled(
      true);
  return specifics;
}

ModelTypeState StateWithEncryption(const std::string& encryption_key_name) {
  ModelTypeState state;
  state.set_initial_sync_done(true);
  state.set_cache_guid(CacheGuidForSuffix(kLocalSuffix));
  state.set_encryption_key_name(encryption_key_name);
  return state;
}

// Creates an EntityData around a copy of the given specifics.
std::unique_ptr<EntityData> SpecificsToEntity(
    const DeviceInfoSpecifics& specifics) {
  auto data = std::make_unique<EntityData>();
  *data->specifics.mutable_device_info() = specifics;
  return data;
}

std::string CacheGuidToTag(const std::string& guid) {
  return "DeviceInfo_" + guid;
}

// Helper method to reduce duplicated code between tests. Wraps the given
// specifics objects in an EntityData and EntityChange of type ACTION_ADD, and
// returns an EntityChangeList containing them all. Order is maintained.
EntityChangeList EntityAddList(
    const std::vector<DeviceInfoSpecifics>& specifics_list) {
  EntityChangeList changes;
  for (const auto& specifics : specifics_list) {
    changes.push_back(EntityChange::CreateAdd(specifics.cache_guid(),
                                              SpecificsToEntity(specifics)));
  }
  return changes;
}

std::map<std::string, sync_pb::EntitySpecifics> DataBatchToSpecificsMap(
    std::unique_ptr<DataBatch> batch) {
  std::map<std::string, sync_pb::EntitySpecifics> storage_key_to_specifics;
  while (batch && batch->HasNext()) {
    const syncer::KeyAndData& pair = batch->Next();
    storage_key_to_specifics[pair.first] = pair.second->specifics;
  }
  return storage_key_to_specifics;
}

class TestLocalDeviceInfoProvider : public MutableLocalDeviceInfoProvider {
 public:
  TestLocalDeviceInfoProvider() = default;
  ~TestLocalDeviceInfoProvider() override = default;

  // MutableLocalDeviceInfoProvider implementation.
  void Initialize(const std::string& cache_guid,
                  const std::string& session_name) override {
    local_device_info_ = std::make_unique<DeviceInfo>(
        cache_guid, session_name, ChromeVersionForSuffix(kLocalSuffix),
        SyncUserAgentForSuffix(kLocalSuffix),
        sync_pb::SyncEnums_DeviceType_TYPE_LINUX,
        SigninScopedDeviceIdForSuffix(kLocalSuffix), base::Time(), true);
  }

  void Clear() override { local_device_info_.reset(); }

  version_info::Channel GetChannel() const override {
    return version_info::Channel::UNKNOWN;
  }

  const DeviceInfo* GetLocalDeviceInfo() const override {
    return local_device_info_.get();
  }

  std::unique_ptr<Subscription> RegisterOnInitializedCallback(
      const base::RepeatingClosure& callback) override {
    NOTIMPLEMENTED();
    return nullptr;
  }

 private:
  std::unique_ptr<DeviceInfo> local_device_info_;

  DISALLOW_COPY_AND_ASSIGN(TestLocalDeviceInfoProvider);
};

class DeviceInfoSyncBridgeTest : public testing::Test,
                                 public DeviceInfoTracker::Observer {
 protected:
  DeviceInfoSyncBridgeTest()
      : store_(ModelTypeStoreTestUtil::CreateInMemoryStoreForTest()) {
    DeviceInfoPrefs::RegisterProfilePrefs(pref_service_.registry());
    ON_CALL(*processor(), IsTrackingMetadata()).WillByDefault(Return(true));
  }

  ~DeviceInfoSyncBridgeTest() override {
    // Some tests may never initialize the bridge.
    if (bridge_)
      bridge_->RemoveObserver(this);

    // Force all remaining (store) tasks to execute so we don't leak memory.
    base::RunLoop().RunUntilIdle();
  }

  void OnDeviceInfoChange() override { change_count_++; }

  // Initialized the bridge based on the current local device and store.
  void InitializeBridge() {
    bridge_ = std::make_unique<DeviceInfoSyncBridge>(
        std::make_unique<TestLocalDeviceInfoProvider>(),
        ModelTypeStoreTestUtil::FactoryForForwardingStore(store_.get()),
        mock_processor_.CreateForwardingProcessor(),
        std::make_unique<DeviceInfoPrefs>(&pref_service_));
    bridge_->AddObserver(this);
  }

  // Creates the bridge and runs any outstanding tasks. This will typically
  // cause all initialization callbacks between the sevice and store to fire.
  void InitializeAndPump() {
    InitializeBridge();
    base::RunLoop().RunUntilIdle();
  }

  // Creates the bridge with no prior data on the store, and mimics sync being
  // enabled by the user with no remote data.
  void InitializeAndMergeInitialData() {
    InitializeAndPump();
    bridge()->OnSyncStarting(TestDataTypeActivationRequest());

    std::unique_ptr<MetadataChangeList> metadata_change_list =
        bridge()->CreateMetadataChangeList();

    metadata_change_list->UpdateModelTypeState(StateWithEncryption(""));
    bridge()->MergeSyncData(std::move(metadata_change_list),
                            EntityChangeList());
  }

  // Allows access to the store before that will ultimately be used to
  // initialize the bridge.
  ModelTypeStore* store() {
    EXPECT_TRUE(store_);
    return store_.get();
  }

  // Get the number of times the bridge notifies observers of changes.
  int change_count() { return change_count_; }

  LocalDeviceInfoProvider* local_device() {
    return bridge_->GetLocalDeviceInfoProvider();
  }

  // Allows access to the bridge after InitializeBridge() is called.
  DeviceInfoSyncBridge* bridge() {
    EXPECT_TRUE(bridge_);
    return bridge_.get();
  }

  MockModelTypeChangeProcessor* processor() { return &mock_processor_; }
  // Should only be called after the bridge has been initialized. Will first
  // recover the bridge's store, so another can be initialized later, and then
  // deletes the bridge.
  void PumpAndShutdown() {
    ASSERT_TRUE(bridge_);
    base::RunLoop().RunUntilIdle();
    bridge_->RemoveObserver(this);
  }

  void RestartBridge() {
    PumpAndShutdown();
    InitializeAndPump();
  }

  void ForcePulse() { bridge()->ForcePulseForTest(); }

  void CommitToStoreAndWait(std::unique_ptr<WriteBatch> batch) {
    base::RunLoop loop;
    store()->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(
            [](base::RunLoop* loop, const base::Optional<ModelError>& result) {
              EXPECT_FALSE(result.has_value()) << result->ToString();
              loop->Quit();
            },
            &loop));
    loop.Run();
  }

  void WriteDataToStore(
      const std::vector<DeviceInfoSpecifics>& specifics_list) {
    std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
    for (auto& specifics : specifics_list) {
      batch->WriteData(specifics.cache_guid(), specifics.SerializeAsString());
    }
    CommitToStoreAndWait(std::move(batch));
  }

  void WriteToStoreWithMetadata(
      const std::vector<DeviceInfoSpecifics>& specifics_list,
      ModelTypeState state) {
    std::unique_ptr<WriteBatch> batch = store()->CreateWriteBatch();
    for (auto& specifics : specifics_list) {
      batch->WriteData(specifics.cache_guid(), specifics.SerializeAsString());
    }
    batch->GetMetadataChangeList()->UpdateModelTypeState(state);
    CommitToStoreAndWait(std::move(batch));
  }

  std::map<std::string, DeviceInfoSpecifics> ReadAllFromStore() {
    std::unique_ptr<ModelTypeStore::RecordList> records;
    base::RunLoop loop;
    store()->ReadAllData(base::BindOnce(
        [](std::unique_ptr<ModelTypeStore::RecordList>* output_records,
           base::RunLoop* loop, const base::Optional<syncer::ModelError>& error,
           std::unique_ptr<ModelTypeStore::RecordList> input_records) {
          EXPECT_FALSE(error) << error->ToString();
          EXPECT_THAT(input_records, NotNull());
          *output_records = std::move(input_records);
          loop->Quit();
        },
        &records, &loop));
    loop.Run();
    std::map<std::string, DeviceInfoSpecifics> result;
    if (records) {
      for (const ModelTypeStore::Record& record : *records) {
        DeviceInfoSpecifics specifics;
        EXPECT_TRUE(specifics.ParseFromString(record.value));
        result.emplace(record.id, specifics);
      }
    }
    return result;
  }

  std::map<std::string, sync_pb::EntitySpecifics> GetAllData() {
    base::RunLoop loop;
    std::unique_ptr<DataBatch> batch;
    bridge_->GetAllDataForDebugging(base::BindOnce(
        [](base::RunLoop* loop, std::unique_ptr<DataBatch>* out_batch,
           std::unique_ptr<DataBatch> batch) {
          *out_batch = std::move(batch);
          loop->Quit();
        },
        &loop, &batch));
    loop.Run();
    EXPECT_NE(nullptr, batch);
    return DataBatchToSpecificsMap(std::move(batch));
  }

  std::map<std::string, sync_pb::EntitySpecifics> GetData(
      const std::vector<std::string>& storage_keys) {
    base::RunLoop loop;
    std::unique_ptr<DataBatch> batch;
    bridge_->GetData(storage_keys, base::BindOnce(
                                       [](base::RunLoop* loop,
                                          std::unique_ptr<DataBatch>* out_batch,
                                          std::unique_ptr<DataBatch> batch) {
                                         *out_batch = std::move(batch);
                                         loop->Quit();
                                       },
                                       &loop, &batch));
    loop.Run();
    EXPECT_NE(nullptr, batch);
    return DataBatchToSpecificsMap(std::move(batch));
  }

 private:
  int change_count_ = 0;

  // In memory model type store needs to be able to post tasks.
  base::test::ScopedTaskEnvironment task_environment_;

  testing::NiceMock<MockModelTypeChangeProcessor> mock_processor_;

  // Holds the store.
  const std::unique_ptr<ModelTypeStore> store_;

  TestingPrefServiceSimple pref_service_;
  // Not initialized immediately (upon test's constructor). This allows each
  // test case to modify the dependencies the bridge will be constructed with.
  std::unique_ptr<DeviceInfoSyncBridge> bridge_;
};

TEST_F(DeviceInfoSyncBridgeTest, BeforeSyncEnabled) {
  InitializeBridge();
  EXPECT_THAT(bridge()->GetAllDeviceInfo(), IsEmpty());
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(bridge()->GetAllDeviceInfo(), IsEmpty());
}

TEST_F(DeviceInfoSyncBridgeTest, GetClientTagNormal) {
  InitializeBridge();
  const std::string guid = "abc";
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info()->set_cache_guid(guid);
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ(CacheGuidToTag(guid), bridge()->GetClientTag(entity_data));
}

TEST_F(DeviceInfoSyncBridgeTest, GetClientTagEmpty) {
  InitializeBridge();
  EntitySpecifics entity_specifics;
  entity_specifics.mutable_device_info();
  EntityData entity_data;
  entity_data.specifics = entity_specifics;
  EXPECT_EQ(CacheGuidToTag(""), bridge()->GetClientTag(entity_data));
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithLocalDataWithoutMetadata) {
  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  WriteDataToStore({specifics});
  InitializeAndPump();

  // Local data without sync metadata should be thrown away.
  EXPECT_TRUE(ReadAllFromStore().empty());
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithLocalMetadata) {
  WriteToStoreWithMetadata(std::vector<DeviceInfoSpecifics>(),
                           StateWithEncryption("ekn"));
  InitializeAndPump();

  // Local metadata without data about the local device is corrupt.
  EXPECT_TRUE(ReadAllFromStore().empty());
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithLocalDataAndMetadata) {
  const DeviceInfoSpecifics local_specifics = CreateSpecifics(kLocalSuffix);
  ModelTypeState state = StateWithEncryption("ekn");
  WriteToStoreWithMetadata({local_specifics}, state);

  EXPECT_CALL(*processor(), ModelReadyToSync(MetadataBatchContains(
                                HasEncryptionKeyName("ekn"),
                                /*entities=*/_)));
  InitializeAndPump();

  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  EXPECT_THAT(*bridge()->GetDeviceInfo(local_specifics.cache_guid()),
              ModelEqualsSpecifics(local_specifics));
  EXPECT_TRUE(bridge()->IsPulseTimerRunningForTest());
}

TEST_F(DeviceInfoSyncBridgeTest, TestWithMultipleLocalDataAndMetadata) {
  const DeviceInfoSpecifics local_specifics = CreateSpecifics(kLocalSuffix);
  const DeviceInfoSpecifics remote_specifics = CreateSpecifics(1);
  ModelTypeState state = StateWithEncryption("ekn");
  WriteToStoreWithMetadata({local_specifics, remote_specifics}, state);

  EXPECT_CALL(*processor(), ModelReadyToSync(MetadataBatchContains(
                                HasEncryptionKeyName("ekn"),
                                /*entities=*/_)));
  InitializeAndPump();

  EXPECT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  EXPECT_THAT(*bridge()->GetDeviceInfo(local_specifics.cache_guid()),
              ModelEqualsSpecifics(local_specifics));
  EXPECT_THAT(*bridge()->GetDeviceInfo(remote_specifics.cache_guid()),
              ModelEqualsSpecifics(remote_specifics));
}

TEST_F(DeviceInfoSyncBridgeTest, GetData) {
  const DeviceInfoSpecifics local_specifics = CreateSpecifics(kLocalSuffix);
  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  const DeviceInfoSpecifics specifics3 = CreateSpecifics(3);
  WriteToStoreWithMetadata(
      {local_specifics, specifics1, specifics2, specifics3},
      StateWithEncryption("ekn"));
  InitializeAndPump();

  EXPECT_THAT(GetData({specifics1.cache_guid()}),
              UnorderedElementsAre(
                  Pair(specifics1.cache_guid(), HasDeviceInfo(specifics1))));

  EXPECT_THAT(GetData({specifics1.cache_guid(), specifics3.cache_guid()}),
              UnorderedElementsAre(
                  Pair(specifics1.cache_guid(), HasDeviceInfo(specifics1)),
                  Pair(specifics3.cache_guid(), HasDeviceInfo(specifics3))));

  EXPECT_THAT(GetData({specifics1.cache_guid(), specifics2.cache_guid(),
                       specifics3.cache_guid()}),
              UnorderedElementsAre(
                  Pair(specifics1.cache_guid(), HasDeviceInfo(specifics1)),
                  Pair(specifics2.cache_guid(), HasDeviceInfo(specifics2)),
                  Pair(specifics3.cache_guid(), HasDeviceInfo(specifics3))));
}

TEST_F(DeviceInfoSyncBridgeTest, GetDataMissing) {
  InitializeAndPump();
  EXPECT_THAT(GetData({"does_not_exist"}), IsEmpty());
}

TEST_F(DeviceInfoSyncBridgeTest, GetAllData) {
  const DeviceInfoSpecifics local_specifics = CreateSpecifics(kLocalSuffix);
  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  WriteToStoreWithMetadata({local_specifics, specifics1, specifics2},
                           StateWithEncryption("ekn"));
  InitializeAndPump();

  EXPECT_THAT(GetAllData(),
              UnorderedElementsAre(
                  Pair(local_device()->GetLocalDeviceInfo()->guid(), _),
                  Pair(specifics1.cache_guid(), HasDeviceInfo(specifics1)),
                  Pair(specifics2.cache_guid(), HasDeviceInfo(specifics2))));
}

TEST_F(DeviceInfoSyncBridgeTest, ApplySyncChangesEmpty) {
  InitializeAndMergeInitialData();
  ASSERT_EQ(1, change_count());

  auto error = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                          EntityChangeList());
  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplySyncChangesInMemory) {
  InitializeAndMergeInitialData();
  ASSERT_EQ(1, change_count());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  auto error_on_add = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), EntityAddList({specifics}));

  EXPECT_FALSE(error_on_add);
  std::unique_ptr<DeviceInfo> info =
      bridge()->GetDeviceInfo(specifics.cache_guid());
  ASSERT_TRUE(info);
  EXPECT_THAT(*info, ModelEqualsSpecifics(specifics));
  EXPECT_EQ(2, change_count());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(
      EntityChange::CreateDelete(specifics.cache_guid()));
  auto error_on_delete = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));

  EXPECT_FALSE(error_on_delete);
  EXPECT_FALSE(bridge()->GetDeviceInfo(specifics.cache_guid()));
  EXPECT_EQ(3, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplySyncChangesStore) {
  InitializeAndMergeInitialData();
  ASSERT_EQ(1, change_count());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  ModelTypeState state = StateWithEncryption("ekn");
  std::unique_ptr<MetadataChangeList> metadata_changes =
      bridge()->CreateMetadataChangeList();
  metadata_changes->UpdateModelTypeState(state);

  auto error = bridge()->ApplySyncChanges(std::move(metadata_changes),
                                          EntityAddList({specifics}));
  EXPECT_FALSE(error);
  EXPECT_EQ(2, change_count());

  EXPECT_CALL(*processor(), ModelReadyToSync(MetadataBatchContains(
                                HasEncryptionKeyName("ekn"),
                                /*entities=*/IsEmpty())));
  RestartBridge();

  std::unique_ptr<DeviceInfo> info =
      bridge()->GetDeviceInfo(specifics.cache_guid());
  ASSERT_TRUE(info);
  EXPECT_THAT(*info, ModelEqualsSpecifics(specifics));
}

TEST_F(DeviceInfoSyncBridgeTest, ApplySyncChangesWithLocalGuid) {
  InitializeAndMergeInitialData();
  ASSERT_EQ(1, change_count());

  ASSERT_TRUE(
      bridge()->GetDeviceInfo(local_device()->GetLocalDeviceInfo()->guid()));
  ASSERT_EQ(1, change_count());

  // The bridge should ignore these changes using this specifics because its
  // guid will match the local device.
  EXPECT_CALL(*processor(), Put(_, _, _)).Times(0);

  const DeviceInfoSpecifics specifics = CreateSpecifics(kLocalSuffix);
  auto error_on_add = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), EntityAddList({specifics}));
  EXPECT_FALSE(error_on_add);
  EXPECT_EQ(1, change_count());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(
      EntityChange::CreateDelete(specifics.cache_guid()));
  auto error_on_delete = bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(), std::move(entity_change_list));
  EXPECT_FALSE(error_on_delete);
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplyDeleteNonexistent) {
  InitializeAndMergeInitialData();
  ASSERT_EQ(1, change_count());

  syncer::EntityChangeList entity_change_list;
  entity_change_list.push_back(EntityChange::CreateDelete("guid"));
  EXPECT_CALL(*processor(), Delete(_, _)).Times(0);
  auto error = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                          std::move(entity_change_list));
  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, MergeEmpty) {
  const std::string kLocalGuid = CacheGuidForSuffix(kLocalSuffix);

  InitializeAndPump();

  ASSERT_FALSE(local_device()->GetLocalDeviceInfo());
  ASSERT_FALSE(bridge()->IsPulseTimerRunningForTest());

  EXPECT_CALL(*processor(), Put(kLocalGuid, _, _));
  EXPECT_CALL(*processor(), Delete(_, _)).Times(0);

  bridge()->OnSyncStarting(TestDataTypeActivationRequest());
  auto error = bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(),
                                       EntityChangeList());
  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());

  ASSERT_TRUE(local_device()->GetLocalDeviceInfo());
  EXPECT_EQ(kLocalGuid, local_device()->GetLocalDeviceInfo()->guid());
  EXPECT_TRUE(bridge()->IsPulseTimerRunningForTest());
}

TEST_F(DeviceInfoSyncBridgeTest, MergeLocalGuid) {
  const std::string kLocalGuid = CacheGuidForSuffix(kLocalSuffix);

  InitializeAndPump();

  ASSERT_FALSE(local_device()->GetLocalDeviceInfo());

  EXPECT_CALL(*processor(), Put(kLocalGuid, _, _));
  EXPECT_CALL(*processor(), Delete(_, _)).Times(0);

  bridge()->OnSyncStarting(TestDataTypeActivationRequest());
  auto error =
      bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(),
                              EntityAddList({CreateSpecifics(kLocalSuffix)}));

  EXPECT_FALSE(error);
  EXPECT_EQ(1, change_count());

  ASSERT_TRUE(local_device()->GetLocalDeviceInfo());
  EXPECT_EQ(kLocalGuid, local_device()->GetLocalDeviceInfo()->guid());
}

TEST_F(DeviceInfoSyncBridgeTest, CountActiveDevices) {
  InitializeAndMergeInitialData();
  // Local device.
  EXPECT_EQ(1, bridge()->CountActiveDevices());

  ON_CALL(*processor(), GetEntityCreationTime(_))
      .WillByDefault(Return(base::Time::Now()));
  ON_CALL(*processor(), GetEntityModificationTime(_))
      .WillByDefault(Return(base::Time::Now()));

  // Regardless of the time, these following two ApplySyncChanges(...) calls
  // have the same guid as the local device.
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({CreateSpecifics(kLocalSuffix)}));
  EXPECT_EQ(1, bridge()->CountActiveDevices());

  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({CreateSpecifics(kLocalSuffix)}));
  EXPECT_EQ(1, bridge()->CountActiveDevices());

  // A different guid will actually contribute to the count.
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({CreateSpecifics(1)}));
  EXPECT_EQ(2, bridge()->CountActiveDevices());

  // Now set time to long ago in the past, it should not be active anymore.
  bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({CreateSpecifics(
          1, base::Time::Now() - base::TimeDelta::FromDays(365))}));
  EXPECT_EQ(1, bridge()->CountActiveDevices());
}

TEST_F(DeviceInfoSyncBridgeTest, CountActiveDevicesWithOverlappingTime) {
  InitializeAndMergeInitialData();
  // Local device.
  ASSERT_EQ(1, bridge()->CountActiveDevices());

  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  const DeviceInfoSpecifics specifics3 = CreateSpecifics(3);

  // Time ranges are overlapping.
  ON_CALL(*processor(), GetEntityCreationTime(specifics1.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(1)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics1.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(3)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics2.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(2)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics2.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(4)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics3.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(2)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics3.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(5)));

  // With two devices, the local device gets ignored because it doesn't overlap.
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({specifics1, specifics2}));

  ASSERT_EQ(3u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(2, bridge()->CountActiveDevices());

  // The third device is also overlapping with the first two (and the local one
  // is still excluded).
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({specifics3}));

  ASSERT_EQ(4u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(3, bridge()->CountActiveDevices());
}

TEST_F(DeviceInfoSyncBridgeTest, CountActiveDevicesWithNonOverlappingTime) {
  InitializeAndMergeInitialData();
  // Local device.
  ASSERT_EQ(1, bridge()->CountActiveDevices());

  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  const DeviceInfoSpecifics specifics3 = CreateSpecifics(3);

  // Time ranges are non-overlapping.
  ON_CALL(*processor(), GetEntityCreationTime(specifics1.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(1)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics1.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(2)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics2.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(3)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics2.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(4)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics3.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(5)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics3.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(6)));

  bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({specifics1, specifics2, specifics3}));

  ASSERT_EQ(4u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(1, bridge()->CountActiveDevices());
}

TEST_F(DeviceInfoSyncBridgeTest,
       CountActiveDevicesWithNonOverlappingTimeAndDistictType) {
  InitializeAndMergeInitialData();
  // Local device.
  ASSERT_EQ(1, bridge()->CountActiveDevices());

  DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  DeviceInfoSpecifics specifics2 = CreateSpecifics(2);
  DeviceInfoSpecifics specifics3 = CreateSpecifics(3);
  // We avoid TYPE_LINUX below to prevent collisions with the local device,
  // exposed as Linux by LocalDeviceInfoProviderMock.
  specifics1.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_PHONE);
  specifics2.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_CROS);
  specifics3.set_device_type(sync_pb::SyncEnums_DeviceType_TYPE_WIN);

  // Time ranges are non-overlapping.
  ON_CALL(*processor(), GetEntityCreationTime(specifics1.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(1)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics1.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(2)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics2.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(3)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics2.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(4)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics3.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(5)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics3.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(6)));

  bridge()->ApplySyncChanges(
      bridge()->CreateMetadataChangeList(),
      EntityAddList({specifics1, specifics2, specifics3}));

  ASSERT_EQ(4u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(4, bridge()->CountActiveDevices());
}

TEST_F(DeviceInfoSyncBridgeTest, CountActiveDevicesWithMalformedTimestamps) {
  InitializeAndMergeInitialData();
  // Local device.
  ASSERT_EQ(1, bridge()->CountActiveDevices());

  const DeviceInfoSpecifics specifics1 = CreateSpecifics(1);
  const DeviceInfoSpecifics specifics2 = CreateSpecifics(2);

  // Time ranges are overlapping.
  ON_CALL(*processor(), GetEntityCreationTime(specifics1.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(1)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics1.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(4)));
  ON_CALL(*processor(), GetEntityCreationTime(specifics2.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(3)));
  ON_CALL(*processor(), GetEntityModificationTime(specifics2.cache_guid()))
      .WillByDefault(
          Return(base::Time::UnixEpoch() + base::TimeDelta::FromMinutes(2)));

  // With two devices, the local device gets ignored because it doesn't overlap.
  bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                             EntityAddList({specifics1, specifics2}));

  ASSERT_EQ(3u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(1, bridge()->CountActiveDevices());
}

TEST_F(DeviceInfoSyncBridgeTest, SendLocalData) {
  // Ensure |last_updated| is about now, plus or minus a little bit.
  EXPECT_CALL(*processor(), Put(_, HasSpecifics(HasLastUpdatedAboutNow()), _));
  InitializeAndMergeInitialData();
  EXPECT_EQ(1, change_count());
  testing::Mock::VerifyAndClearExpectations(processor());

  // Ensure |last_updated| is about now, plus or minus a little bit.
  EXPECT_CALL(*processor(), Put(_, HasSpecifics(HasLastUpdatedAboutNow()), _));
  ForcePulse();
  EXPECT_EQ(2, change_count());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplyStopSyncChangesWithClearData) {
  InitializeAndMergeInitialData();
  ASSERT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  ASSERT_EQ(1, change_count());
  ASSERT_FALSE(ReadAllFromStore().empty());
  ASSERT_TRUE(bridge()->IsPulseTimerRunningForTest());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  auto error = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                          EntityAddList({specifics}));

  ASSERT_FALSE(error);
  ASSERT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  ASSERT_EQ(2, change_count());

  // Should clear out all local data and notify observers.
  bridge()->ApplyStopSyncChanges(bridge()->CreateMetadataChangeList());
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(3, change_count());
  EXPECT_TRUE(ReadAllFromStore().empty());
  EXPECT_FALSE(bridge()->IsPulseTimerRunningForTest());

  // Reloading from storage shouldn't contain remote data.
  RestartBridge();
  EXPECT_EQ(0u, bridge()->GetAllDeviceInfo().size());

  // If sync is re-enabled and the remote data is now empty, we shouldn't
  // contain remote data.
  bridge()->OnSyncStarting(TestDataTypeActivationRequest());
  bridge()->MergeSyncData(bridge()->CreateMetadataChangeList(),
                          EntityChangeList());
  // Local device.
  EXPECT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  EXPECT_TRUE(bridge()->IsPulseTimerRunningForTest());
}

TEST_F(DeviceInfoSyncBridgeTest, ApplyStopSyncChangesWithKeepData) {
  InitializeAndMergeInitialData();
  ASSERT_EQ(1u, bridge()->GetAllDeviceInfo().size());
  ASSERT_EQ(1, change_count());
  ASSERT_FALSE(ReadAllFromStore().empty());
  ASSERT_TRUE(bridge()->IsPulseTimerRunningForTest());

  const DeviceInfoSpecifics specifics = CreateSpecifics(1);
  auto error = bridge()->ApplySyncChanges(bridge()->CreateMetadataChangeList(),
                                          EntityAddList({specifics}));

  ASSERT_FALSE(error);
  ASSERT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  ASSERT_EQ(2, change_count());

  // Should clear out all local data and notify observers.
  bridge()->ApplyStopSyncChanges(/*delete_metadata_change_list=*/nullptr);
  EXPECT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  EXPECT_EQ(2, change_count());
  EXPECT_FALSE(ReadAllFromStore().empty());
  EXPECT_TRUE(bridge()->IsPulseTimerRunningForTest());

  // Reloading from storage should still contain remote data.
  RestartBridge();
  EXPECT_EQ(2u, bridge()->GetAllDeviceInfo().size());
  EXPECT_TRUE(bridge()->IsPulseTimerRunningForTest());
}

}  // namespace

}  // namespace syncer
