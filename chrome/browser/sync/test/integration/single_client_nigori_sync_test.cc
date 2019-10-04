// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/sync/base/time.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

using encryption_helper::GetServerNigori;
using encryption_helper::SetNigoriInFakeServer;

struct KeyParams {
  syncer::KeyDerivationParams derivation_params;
  std::string password;
};

KeyParams KeystoreKeyParams(const std::string& key) {
  // Due to mis-encode of keystore keys to base64 we have to always encode such
  // keys to provide backward compatibility.
  std::string encoded_key;
  base::Base64Encode(key, &encoded_key);
  return {syncer::KeyDerivationParams::CreateForPbkdf2(),
          std::move(encoded_key)};
}

// Builds NigoriSpecifics with following fields:
// 1. encryption_keybag contains all keys derived from |keybag_keys_params|
// and encrypted with a key derived from |keybag_decryptor_params|.
// keystore_decryptor_token is always saved in encryption_keybag, even if it
// is not derived from any params in |keybag_keys_params|.
// 2. keystore_decryptor_token contains the key derived from
// |keybag_decryptor_params| and encrypted with a key derived from
// |keystore_key_params|.
// 3. passphrase_type is KEYSTORE_PASSHPRASE.
// 4. Other fields are default.
sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecifics(
    const std::vector<KeyParams>& keybag_keys_params,
    const KeyParams& keystore_decryptor_params,
    const KeyParams& keystore_key_params) {
  sync_pb::NigoriSpecifics specifics;

  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          keystore_decryptor_params.password,
          keystore_decryptor_params.derivation_params);
  for (const KeyParams& key_params : keybag_keys_params) {
    cryptographer->EmplaceKey(key_params.password,
                              key_params.derivation_params);
  }

  EXPECT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));

  std::string serialized_keystore_decryptor =
      cryptographer->ExportDefaultKey().SerializeAsString();

  std::unique_ptr<syncer::CryptographerImpl> keystore_cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(
          keystore_key_params.password, keystore_key_params.derivation_params);
  EXPECT_TRUE(keystore_cryptographer->EncryptString(
      serialized_keystore_decryptor,
      specifics.mutable_keystore_decryptor_token()));

  specifics.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  specifics.set_keystore_migration_time(
      syncer::TimeToProtoTime(base::Time::Now()));
  return specifics;
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

  bool WaitForPasswordForms(
      const std::vector<autofill::PasswordForm>& forms) const {
    return PasswordFormsChecker(0, forms).Wait();
  }

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

// Tests that client can decrypt passwords, encrypted with keystore key in case
// Nigori node contains only this key. We first inject keystore Nigori and
// encrypted password form to fake server and then check that client
// successfully received and decrypted this password form.
IN_PROC_BROWSER_TEST_P(SingleClientNigoriSyncTestWithUssTests,
                       ShouldDecryptWithKeystoreNigori) {
  const std::vector<std::string>& keystore_keys =
      GetFakeServer()->GetKeystoreKeys();
  ASSERT_TRUE(keystore_keys.size() == 1);
  const KeyParams kKeystoreKeyParams = KeystoreKeyParams(keystore_keys.back());
  SetNigoriInFakeServer(GetFakeServer(),
                        BuildKeystoreNigoriSpecifics(
                            /*keybag_keys_params=*/{kKeystoreKeyParams},
                            /*keystore_decryptor_params=*/kKeystoreKeyParams,
                            /*keystore_key_params=*/kKeystoreKeyParams));

  const autofill::PasswordForm password_form =
      passwords_helper::CreateTestPasswordForm(0);
  passwords_helper::InjectEncryptedServerPassword(
      password_form, kKeystoreKeyParams.password,
      kKeystoreKeyParams.derivation_params, GetFakeServer());
  ASSERT_TRUE(SetupSync());
  EXPECT_TRUE(WaitForPasswordForms({password_form}));
}

INSTANTIATE_TEST_SUITE_P(USS,
                         SingleClientNigoriSyncTestWithUssTests,
                         ::testing::Values(false, true));

}  // namespace
