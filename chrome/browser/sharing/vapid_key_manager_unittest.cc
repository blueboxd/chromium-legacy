// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/vapid_key_manager.h"

#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "crypto/ec_private_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class VapidKeyManagerTest : public testing::Test {
 protected:
  VapidKeyManagerTest()
      : sharing_sync_preference_(&prefs_),
        vapid_key_manager_(&sharing_sync_preference_) {
    SharingSyncPreference::RegisterProfilePrefs(prefs_.registry());
  }

  SharingSyncPreference sharing_sync_preference_;
  sync_preferences::TestingPrefServiceSyncable prefs_;
  VapidKeyManager vapid_key_manager_;
};

}  // namespace

TEST_F(VapidKeyManagerTest, GetOrCreateKey) {
  // No keys stored in preferences.
  EXPECT_EQ(base::nullopt, sharing_sync_preference_.GetVapidKey());

  // Expected to create new keys and store in preferences.
  crypto::ECPrivateKey* key = vapid_key_manager_.GetOrCreateKey();
  std::vector<uint8_t> stored_key;
  EXPECT_TRUE(key->ExportPrivateKey(&stored_key));
  EXPECT_EQ(stored_key, sharing_sync_preference_.GetVapidKey());

  // Expected to return old keys from preferences.
  std::vector<uint8_t> temp_key;
  EXPECT_TRUE(vapid_key_manager_.GetOrCreateKey()->ExportPrivateKey(&temp_key));
  EXPECT_EQ(temp_key, stored_key);
}
