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
const std::set<sync_pb::SharingSpecificFields::EnabledFeatures>
    kClickToCallEnabled{sync_pb::SharingSpecificFields::CLICK_TO_CALL};

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
                                      kDeviceAuthToken, kClickToCallEnabled));
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
  EXPECT_NE(base::nullopt,
            sharing_sync_preference_.GetSyncedDevice(kDeviceGuid));
  sharing_sync_preference_.RemoveDevice(kDeviceGuid);
  EXPECT_EQ(base::nullopt,
            sharing_sync_preference_.GetSyncedDevice(kDeviceGuid));
}

TEST_F(SharingSyncPreferenceTest, SyncDevice) {
  EXPECT_EQ(base::nullopt,
            sharing_sync_preference_.GetSyncedDevice(kDeviceGuid));
  SyncDefaultDevice();
  base::Optional<SharingSyncPreference::Device> device =
      sharing_sync_preference_.GetSyncedDevice(kDeviceGuid);

  EXPECT_NE(base::nullopt, device);
  EXPECT_EQ(kDeviceFcmToken, device->fcm_token);
  EXPECT_EQ(kDeviceP256dh, device->p256dh);
  EXPECT_EQ(kDeviceAuthToken, device->auth_secret);
  EXPECT_EQ(kClickToCallEnabled, device->enabled_features);

  auto synced_devices = sharing_sync_preference_.GetSyncedDevices();
  auto it = synced_devices.find(kDeviceGuid);
  EXPECT_NE(synced_devices.end(), it);
  EXPECT_EQ(device->fcm_token, it->second.fcm_token);
  EXPECT_EQ(device->p256dh, it->second.p256dh);
  EXPECT_EQ(device->auth_secret, it->second.auth_secret);
  EXPECT_EQ(device->enabled_features, it->second.enabled_features);
}

TEST_F(SharingSyncPreferenceTest, FCMRegistrationGetSet) {
  EXPECT_FALSE(sharing_sync_preference_.GetFCMRegistration());

  base::Time time_now = base::Time::Now();
  sharing_sync_preference_.SetFCMRegistration(
      SharingSyncPreference::FCMRegistration(kAuthorizedEntity, kDeviceFcmToken,
                                             kDeviceP256dh, kDeviceAuthToken,
                                             time_now));

  auto fcm_registration = sharing_sync_preference_.GetFCMRegistration();
  EXPECT_TRUE(fcm_registration);
  EXPECT_EQ(kAuthorizedEntity, fcm_registration->authorized_entity);
  EXPECT_EQ(kDeviceFcmToken, fcm_registration->fcm_token);
  EXPECT_EQ(kDeviceP256dh, fcm_registration->p256dh);
  EXPECT_EQ(kDeviceAuthToken, fcm_registration->auth_secret);
  EXPECT_EQ(time_now, fcm_registration->timestamp);

  sharing_sync_preference_.ClearFCMRegistration();
  EXPECT_FALSE(sharing_sync_preference_.GetFCMRegistration());
}
