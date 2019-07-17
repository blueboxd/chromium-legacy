// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/sharing_sync_preference.h"

#include <memory>

#include "base/guid.h"
#include "base/time/time.h"
#include "base/value_conversions.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kVapidKeyStr[] = "test_vapid_key";
const std::vector<uint8_t> kVapidKey =
    std::vector<uint8_t>(std::begin(kVapidKeyStr), std::end(kVapidKeyStr));

const char kDeviceGuid[] = "test_device";
const char kDeviceFcmToken[] = "test_fcm_token";
const char kDeviceAuthToken[] = "test_auth_token";
const char kDeviceP256dh[] = "test_p256dh";
const int kCapabilities = 1;

const char kAuthorizedEntity[] = "authorized_entity";

}  // namespace

class SharingSyncPreferenceTest : public testing::Test {
 protected:
  SharingSyncPreferenceTest() : sharing_sync_preference_(&prefs_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  void SyncDefaultDevice() {
    sharing_sync_preference_.SetSyncDevice(
        kDeviceGuid,
        SharingSyncPreference::Device(kDeviceFcmToken, kDeviceP256dh,
                                      kDeviceAuthToken, kCapabilities));
  }

  base::Optional<SharingSyncPreference::Device> GetDevice(
      const std::string& guid) {
    std::map<std::string, SharingSyncPreference::Device> synced_devices =
        sharing_sync_preference_.GetSyncedDevices();
    auto it = synced_devices.find(guid);
    if (it == synced_devices.end())
      return base::nullopt;
    else
      return std::move(it->second);
  }

  static base::Value CreateRandomDevice(base::Time timestamp) {
    return SharingSyncPreference::DeviceToValue(
        {base::GenerateGUID(), kDeviceP256dh, kDeviceAuthToken, kCapabilities},
        timestamp);
  }

  sync_preferences::TestingPrefServiceSyncable prefs_;
  SharingSyncPreference sharing_sync_preference_;
};

TEST_F(SharingSyncPreferenceTest, UpdateVapidKeys) {
  EXPECT_EQ(base::nullopt, sharing_sync_preference_.GetVapidKey());
  sharing_sync_preference_.SetVapidKey(kVapidKey);
  EXPECT_EQ(kVapidKey, sharing_sync_preference_.GetVapidKey());
}

TEST_F(SharingSyncPreferenceTest, RemoveDevice) {
  SyncDefaultDevice();
  EXPECT_NE(base::nullopt, GetDevice(kDeviceGuid));
  sharing_sync_preference_.RemoveDevice(kDeviceGuid);
  EXPECT_EQ(base::nullopt, GetDevice(kDeviceGuid));
}

TEST_F(SharingSyncPreferenceTest, SyncDevice) {
  EXPECT_EQ(base::nullopt, GetDevice(kDeviceGuid));
  SyncDefaultDevice();
  base::Optional<SharingSyncPreference::Device> device = GetDevice(kDeviceGuid);

  EXPECT_NE(base::nullopt, device);
  EXPECT_EQ(kDeviceFcmToken, device->fcm_token);
  EXPECT_EQ(kDeviceP256dh, device->p256dh);
  EXPECT_EQ(kDeviceAuthToken, device->auth_secret);
  EXPECT_EQ(kCapabilities, device->capabilities);
}

TEST_F(SharingSyncPreferenceTest, MergeVapidKeys_BothEmpty) {
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);
  auto merged = SharingSyncPreference::MaybeMergeVapidKey(local, server);
  EXPECT_EQ(nullptr, merged);
}

TEST_F(SharingSyncPreferenceTest, MergeVapidKeys_ServerEmpty) {
  base::Time time = base::Time::Now();
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);

  local.SetKey("vapid_creation_timestamp", base::CreateTimeValue(time));

  auto merged = SharingSyncPreference::MaybeMergeVapidKey(local, server);
  ASSERT_TRUE(merged);
  EXPECT_EQ(local, *merged);
}

TEST_F(SharingSyncPreferenceTest, MergeVapidKeys_LocalEmpty) {
  base::Time time = base::Time::Now();
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);

  server.SetKey("vapid_creation_timestamp", base::CreateTimeValue(time));

  auto merged = SharingSyncPreference::MaybeMergeVapidKey(local, server);
  EXPECT_EQ(nullptr, merged);
}

TEST_F(SharingSyncPreferenceTest, MergeVapidKeys_LocalNewer) {
  base::Time old_time = base::Time::Now();
  base::Time new_time = old_time + base::TimeDelta::FromSeconds(1);
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);

  local.SetKey("vapid_creation_timestamp", base::CreateTimeValue(new_time));
  server.SetKey("vapid_creation_timestamp", base::CreateTimeValue(old_time));

  auto merged = SharingSyncPreference::MaybeMergeVapidKey(local, server);
  EXPECT_EQ(nullptr, merged);
}

TEST_F(SharingSyncPreferenceTest, MergeVapidKeys_ServerNewer) {
  base::Time old_time = base::Time::Now();
  base::Time new_time = old_time + base::TimeDelta::FromSeconds(1);
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);

  local.SetKey("vapid_creation_timestamp", base::CreateTimeValue(old_time));
  server.SetKey("vapid_creation_timestamp", base::CreateTimeValue(new_time));

  auto merged = SharingSyncPreference::MaybeMergeVapidKey(local, server);
  ASSERT_TRUE(merged);
  EXPECT_EQ(local, *merged);
}

TEST_F(SharingSyncPreferenceTest, MergeVapidKeys_BothEqual) {
  base::Time time = base::Time::Now();
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);

  local.SetKey("vapid_creation_timestamp", base::CreateTimeValue(time));
  server.SetKey("vapid_creation_timestamp", base::CreateTimeValue(time));

  auto merged = SharingSyncPreference::MaybeMergeVapidKey(local, server);
  EXPECT_EQ(nullptr, merged);
}

TEST_F(SharingSyncPreferenceTest, MergeDevices_BothEmpty) {
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);
  auto merged = SharingSyncPreference::MaybeMergeSyncedDevices(local, server);
  EXPECT_EQ(nullptr, merged);
}

TEST_F(SharingSyncPreferenceTest, MergeDevices_ServerEmpty) {
  base::Time time = base::Time::Now();
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);

  local.SetKey("device-1", CreateRandomDevice(time));

  auto merged = SharingSyncPreference::MaybeMergeSyncedDevices(local, server);
  ASSERT_TRUE(merged);
  EXPECT_EQ(local, *merged);
}

TEST_F(SharingSyncPreferenceTest, MergeDevices_LocalEmpty) {
  base::Time time = base::Time::Now();
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);

  server.SetKey("device-1", CreateRandomDevice(time));

  auto merged = SharingSyncPreference::MaybeMergeSyncedDevices(local, server);
  EXPECT_EQ(nullptr, merged);
}

TEST_F(SharingSyncPreferenceTest, MergeDevices_BothValues) {
  base::Time old_time = base::Time::Now();
  base::Time new_time = old_time + base::TimeDelta::FromSeconds(1);
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);

  local.SetKey("local_only", CreateRandomDevice(old_time));
  local.SetKey("server_newer", CreateRandomDevice(old_time));
  server.SetKey("server_newer", CreateRandomDevice(new_time));
  local.SetKey("local_newer", CreateRandomDevice(new_time));
  server.SetKey("local_newer", CreateRandomDevice(old_time));
  local.SetKey("both_same", CreateRandomDevice(old_time));
  server.SetKey("both_same", CreateRandomDevice(old_time));
  server.SetKey("server_only", CreateRandomDevice(old_time));

  auto merged = SharingSyncPreference::MaybeMergeSyncedDevices(local, server);
  ASSERT_TRUE(merged);

  ASSERT_TRUE(merged->FindKey("local_only"));
  EXPECT_EQ(*local.FindKey("local_only"), *merged->FindKey("local_only"));
  ASSERT_TRUE(merged->FindKey("server_newer"));
  EXPECT_EQ(*server.FindKey("server_newer"), *merged->FindKey("server_newer"));
  ASSERT_TRUE(merged->FindKey("local_newer"));
  EXPECT_EQ(*local.FindKey("local_newer"), *merged->FindKey("local_newer"));
  ASSERT_TRUE(merged->FindKey("both_same"));
  EXPECT_EQ(*server.FindKey("both_same"), *merged->FindKey("both_same"));
  ASSERT_TRUE(merged->FindKey("server_only"));
  EXPECT_EQ(*server.FindKey("server_only"), *merged->FindKey("server_only"));
}

TEST_F(SharingSyncPreferenceTest, MergeDevices_ServerOnlyOrNewer) {
  base::Time old_time = base::Time::Now();
  base::Time new_time = old_time + base::TimeDelta::FromSeconds(1);
  base::Value local(base::Value::Type::DICTIONARY);
  base::Value server(base::Value::Type::DICTIONARY);

  local.SetKey("server_newer", CreateRandomDevice(old_time));
  server.SetKey("server_newer", CreateRandomDevice(new_time));
  server.SetKey("server_only", CreateRandomDevice(old_time));

  auto merged = SharingSyncPreference::MaybeMergeSyncedDevices(local, server);
  EXPECT_EQ(nullptr, merged);
}

TEST_F(SharingSyncPreferenceTest, FCMRegistrationGetSet) {
  EXPECT_FALSE(sharing_sync_preference_.GetFCMRegistration());

  base::Time time_now = base::Time::Now();
  sharing_sync_preference_.SetFCMRegistration(
      {kAuthorizedEntity, kDeviceFcmToken, time_now});

  auto fcm_registration = sharing_sync_preference_.GetFCMRegistration();
  EXPECT_TRUE(fcm_registration);
  EXPECT_EQ(kAuthorizedEntity, fcm_registration->authorized_entity);
  EXPECT_EQ(kDeviceFcmToken, fcm_registration->fcm_token);
  EXPECT_EQ(time_now, fcm_registration->timestamp);

  sharing_sync_preference_.ClearFCMRegistration();
  EXPECT_FALSE(sharing_sync_preference_.GetFCMRegistration());
}
