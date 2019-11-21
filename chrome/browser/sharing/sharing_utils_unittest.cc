// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_utils.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/features.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class SharingUtilsTest : public testing::Test {
 public:
  SharingUtilsTest() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  syncer::TestSyncService test_sync_service_;
};

}  // namespace


TEST_F(SharingUtilsTest, SyncEnabled_SigninOnly) {
  // Enable transport mode required features.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kSharingUseDeviceInfo, kSharingDeriveVapidKey},
      /*disabled_features=*/{});
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes({syncer::DEVICE_INFO});

  EXPECT_TRUE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_FALSE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncEnabled_FullySynced) {
  // Disable transport mode required features.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kSharingUseDeviceInfo, kSharingDeriveVapidKey});
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  EXPECT_TRUE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_FALSE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_SigninOnly_MissingDataTypes) {
  // Enable transport mode required features.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kSharingUseDeviceInfo, kSharingDeriveVapidKey},
      /*disabled_features=*/{});
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes({});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_TRUE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_FullySynced_MissingDataTypes) {
  // Disable transport mode required features.
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{kSharingUseDeviceInfo, kSharingDeriveVapidKey});
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  test_sync_service_.SetActiveDataTypes({syncer::DEVICE_INFO});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_TRUE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_Disabled) {
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::DISABLED);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_TRUE(IsSyncDisabledForSharing(&test_sync_service_));
}

TEST_F(SharingUtilsTest, SyncDisabled_Configuring) {
  test_sync_service_.SetTransportState(
      syncer::SyncService::TransportState::CONFIGURING);
  test_sync_service_.SetActiveDataTypes(
      {syncer::DEVICE_INFO, syncer::PREFERENCES});

  EXPECT_FALSE(IsSyncEnabledForSharing(&test_sync_service_));
  EXPECT_FALSE(IsSyncDisabledForSharing(&test_sync_service_));
}
