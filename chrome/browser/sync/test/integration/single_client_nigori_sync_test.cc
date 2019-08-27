// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/nigori/nigori.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using encryption_helper::GetServerNigori;

syncer::KeyParams KeystoreKeyParams(const std::string& key) {
  // Due to mis-encode of keystore keys to base64 we have to always encode such
  // keys to provide backward compatibility.
  std::string encoded_key;
  base::Base64Encode(key, &encoded_key);
  return {syncer::KeyDerivationParams::CreateForPbkdf2(),
          std::move(encoded_key)};
}

MATCHER_P(IsDataEncryptedWith, key_params, "") {
  const sync_pb::EncryptedData& encrypted_data = arg;
  std::unique_ptr<syncer::Nigori> nigori = syncer::Nigori::CreateByDerivation(
      key_params.derivation_params, key_params.password);
  std::string nigori_name;
  EXPECT_TRUE(nigori->Permute(syncer::Nigori::Type::Password,
                              syncer::kNigoriKeyName, &nigori_name));
  return encrypted_data.key_name() == nigori_name;
}

class SingleClientNigoriSyncTestWithUssTests
    : public SyncTest,
      public testing::WithParamInterface<bool> {
 public:
  SingleClientNigoriSyncTestWithUssTests() : SyncTest(SINGLE_CLIENT) {
    if (GetParam()) {
      // USS Nigori requires USS implementations to be enabled for all
      // datatypes.
      override_features_.InitWithFeatures(
          /*enabled_features=*/{switches::kSyncUSSBookmarks,
                                switches::kSyncUSSPasswords,
                                switches::kSyncUSSNigori},
          /*disabled_features=*/{});
    } else {
      // We test Directory Nigori with default values of USS feature flags of
      // other datatypes.
      override_features_.InitAndDisableFeature(switches::kSyncUSSNigori);
    }
  }
  ~SingleClientNigoriSyncTestWithUssTests() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientNigoriSyncTestWithUssTests);

  base::test::ScopedFeatureList override_features_;
};

IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithUssTests,
                       ShouldCommitKeystoreNigoriWhenReceivedDefault) {
  // SetupSync() should make FakeServer send default NigoriSpecifics.
  ASSERT_TRUE(SetupSync());
  // TODO(crbug/922900): we may want to actually wait for specifics update in
  // fake server. Due to implementation details it's not currently needed.
  sync_pb::NigoriSpecifics specifics;
  EXPECT_TRUE(GetServerNigori(GetFakeServer(), &specifics));

  const std::vector<std::string>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_TRUE(keystore_keys.size() == 1);
  EXPECT_THAT(specifics.encryption_keybag(),
              IsDataEncryptedWith(KeystoreKeyParams(keystore_keys.back())));
  EXPECT_EQ(specifics.passphrase_type(),
            sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  EXPECT_TRUE(specifics.keybag_is_frozen());
  EXPECT_TRUE(specifics.has_keystore_migration_time());
}

INSTANTIATE_TEST_SUITE_P(USS,
                         SingleClientNigoriSyncTestWithUssTests,
                         ::testing::Values(false, true));

}  // namespace
