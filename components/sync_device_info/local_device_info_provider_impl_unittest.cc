// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_provider_impl.h"

#include "base/memory/ptr_util.h"
#include "components/sync/driver/sync_util.h"
#include "components/sync_device_info/device_info_sync_client.h"
#include "components/version_info/version_string.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

const char kLocalDeviceGuid[] = "foo";
const char kLocalDeviceSessionName[] = "bar";

const char kSharingFCMToken[] = "test_fcm_token";
const char kSharingP256dh[] = "test_p256_dh";
const char kSharingAuthSecret[] = "test_auth_secret";
const sync_pb::SharingSpecificFields::EnabledFeatures
    kSharingEnabledFeatures[] = {sync_pb::SharingSpecificFields::CLICK_TO_CALL};

using testing::NotNull;
using testing::Return;

class MockDeviceInfoSyncClient : public DeviceInfoSyncClient {
 public:
  MockDeviceInfoSyncClient() = default;
  ~MockDeviceInfoSyncClient() = default;

  MOCK_CONST_METHOD0(GetSigninScopedDeviceId, std::string());
  MOCK_CONST_METHOD0(GetSendTabToSelfReceivingEnabled, bool());
  MOCK_CONST_METHOD0(GetLocalSharingInfo,
                     base::Optional<DeviceInfo::SharingInfo>());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockDeviceInfoSyncClient);
};

class LocalDeviceInfoProviderImplTest : public testing::Test {
 public:
  LocalDeviceInfoProviderImplTest() {}
  ~LocalDeviceInfoProviderImplTest() override {}

  void SetUp() override {
    provider_ = std::make_unique<LocalDeviceInfoProviderImpl>(
        version_info::Channel::UNKNOWN,
        version_info::GetVersionStringWithModifier("UNKNOWN"),
        &device_info_sync_client_);
  }

  void TearDown() override { provider_.reset(); }

 protected:
  void InitializeProvider() { InitializeProvider(kLocalDeviceGuid); }

  void InitializeProvider(const std::string& guid) {
    provider_->Initialize(guid, kLocalDeviceSessionName);
  }

  testing::NiceMock<MockDeviceInfoSyncClient> device_info_sync_client_;
  std::unique_ptr<LocalDeviceInfoProviderImpl> provider_;
};

TEST_F(LocalDeviceInfoProviderImplTest, GetLocalDeviceInfo) {
  ASSERT_EQ(nullptr, provider_->GetLocalDeviceInfo());

  InitializeProvider();

  const DeviceInfo* local_device_info = provider_->GetLocalDeviceInfo();
  ASSERT_NE(nullptr, local_device_info);
  EXPECT_EQ(std::string(kLocalDeviceGuid), local_device_info->guid());
  EXPECT_EQ(kLocalDeviceSessionName, local_device_info->client_name());
  EXPECT_EQ(MakeUserAgentForSync(provider_->GetChannel()),
            local_device_info->sync_user_agent());

  provider_->Clear();
  ASSERT_EQ(nullptr, provider_->GetLocalDeviceInfo());
}

TEST_F(LocalDeviceInfoProviderImplTest, GetSigninScopedDeviceId) {
  const std::string kSigninScopedDeviceId = "device_id";

  EXPECT_CALL(device_info_sync_client_, GetSigninScopedDeviceId())
      .WillOnce(Return(kSigninScopedDeviceId));

  InitializeProvider();

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_EQ(kSigninScopedDeviceId,
            provider_->GetLocalDeviceInfo()->signin_scoped_device_id());
}

TEST_F(LocalDeviceInfoProviderImplTest, SendTabToSelfReceivingEnabled) {
  ON_CALL(device_info_sync_client_, GetSendTabToSelfReceivingEnabled())
      .WillByDefault(Return(true));

  InitializeProvider();

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_TRUE(
      provider_->GetLocalDeviceInfo()->send_tab_to_self_receiving_enabled());

  ON_CALL(device_info_sync_client_, GetSendTabToSelfReceivingEnabled())
      .WillByDefault(Return(false));

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_FALSE(
      provider_->GetLocalDeviceInfo()->send_tab_to_self_receiving_enabled());
}

TEST_F(LocalDeviceInfoProviderImplTest, SharingInfo) {
  ON_CALL(device_info_sync_client_, GetLocalSharingInfo())
      .WillByDefault(Return(base::nullopt));

  InitializeProvider();

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  EXPECT_FALSE(provider_->GetLocalDeviceInfo()->sharing_info());

  std::set<sync_pb::SharingSpecificFields::EnabledFeatures> enabled_features(
      std::begin(kSharingEnabledFeatures), std::end(kSharingEnabledFeatures));
  base::Optional<DeviceInfo::SharingInfo> sharing_info =
      base::make_optional<DeviceInfo::SharingInfo>(
          kSharingFCMToken, kSharingP256dh, kSharingAuthSecret,
          enabled_features);
  ON_CALL(device_info_sync_client_, GetLocalSharingInfo())
      .WillByDefault(Return(sharing_info));

  ASSERT_THAT(provider_->GetLocalDeviceInfo(), NotNull());
  const base::Optional<DeviceInfo::SharingInfo>& local_sharing_info =
      provider_->GetLocalDeviceInfo()->sharing_info();
  EXPECT_TRUE(local_sharing_info);
  EXPECT_EQ(kSharingFCMToken, local_sharing_info->fcm_token);
  EXPECT_EQ(kSharingP256dh, local_sharing_info->p256dh);
  EXPECT_EQ(kSharingAuthSecret, local_sharing_info->auth_secret);
  EXPECT_EQ(enabled_features, local_sharing_info->enabled_features);
}

}  // namespace
}  // namespace syncer
