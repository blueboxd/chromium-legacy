// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "chrome/browser/sync/test/integration/encryption_helper.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/base/sync_base_switches.h"
#include "components/sync/base/time.h"
#include "components/sync/driver/profile_sync_service.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/engine/sync_engine_switches.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/nigori_key_bag.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace encryption_helper {

namespace {

sync_pb::NigoriSpecifics BuildKeystoreNigoriSpecifics(
    const std::string& passphrase) {
  syncer::KeyDerivationParams key_derivation_params =
      syncer::KeyDerivationParams::CreateForPbkdf2();

  std::unique_ptr<syncer::CryptographerImpl> cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(passphrase,
                                                         key_derivation_params);
  cryptographer->EmplaceKey(passphrase, key_derivation_params);

  sync_pb::NigoriSpecifics specifics;
  EXPECT_TRUE(cryptographer->Encrypt(cryptographer->ToProto().key_bag(),
                                     specifics.mutable_encryption_keybag()));

  std::string serialized_keystore_decryptor =
      cryptographer->ExportDefaultKey().SerializeAsString();

  std::unique_ptr<syncer::CryptographerImpl> keystore_cryptographer =
      syncer::CryptographerImpl::FromSingleKeyForTesting(passphrase,
                                                         key_derivation_params);
  EXPECT_TRUE(keystore_cryptographer->EncryptString(
      serialized_keystore_decryptor,
      specifics.mutable_keystore_decryptor_token()));

  specifics.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
  specifics.set_keystore_migration_time(
      syncer::TimeToProtoTime(base::Time::Now()));
  return specifics;
}

}  // namespace

bool GetServerNigori(fake_server::FakeServer* fake_server,
                     sync_pb::NigoriSpecifics* nigori) {
  std::vector<sync_pb::SyncEntity> entity_list =
      fake_server->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  if (entity_list.size() != 1U) {
    return false;
  }

  *nigori = entity_list[0].specifics().nigori();
  return true;
}

std::unique_ptr<syncer::Cryptographer>
InitCustomPassphraseCryptographerFromNigori(
    const sync_pb::NigoriSpecifics& nigori,
    const std::string& passphrase) {
  std::unique_ptr<syncer::CryptographerImpl> cryptographer;
  sync_pb::EncryptedData keybag = nigori.encryption_keybag();

  std::string decoded_salt;
  switch (syncer::ProtoKeyDerivationMethodToEnum(
      nigori.custom_passphrase_key_derivation_method())) {
    case syncer::KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      cryptographer =
          syncer::CryptographerImpl::FromSingleKeyForTesting(passphrase);
      break;
    case syncer::KeyDerivationMethod::SCRYPT_8192_8_11:
      EXPECT_TRUE(base::Base64Decode(
          nigori.custom_passphrase_key_derivation_salt(), &decoded_salt));
      cryptographer = syncer::CryptographerImpl::FromSingleKeyForTesting(
          passphrase,
          syncer::KeyDerivationParams::CreateForScrypt(decoded_salt));
      break;
    case syncer::KeyDerivationMethod::UNSUPPORTED:
      // This test cannot pass since we wouldn't know how to decrypt data
      // encrypted using an unsupported method.
      ADD_FAILURE() << "Unsupported key derivation method encountered: "
                    << nigori.custom_passphrase_key_derivation_method();
      return syncer::CryptographerImpl::CreateEmpty();
  }

  std::string decrypted_keys_str;
  EXPECT_TRUE(cryptographer->DecryptToString(nigori.encryption_keybag(),
                                             &decrypted_keys_str));

  sync_pb::NigoriKeyBag decrypted_keys;
  EXPECT_TRUE(decrypted_keys.ParseFromString(decrypted_keys_str));

  syncer::NigoriKeyBag key_bag =
      syncer::NigoriKeyBag::CreateFromProto(decrypted_keys);

  cryptographer->EmplaceKeysFrom(key_bag);
  return cryptographer;
}

sync_pb::NigoriSpecifics CreateCustomPassphraseNigori(
    const KeyParams& passphrase_key_params,
    const base::Optional<KeyParams>& old_key_params) {
  syncer::KeyDerivationMethod method =
      passphrase_key_params.derivation_params.method();

  sync_pb::NigoriSpecifics nigori;
  nigori.set_keybag_is_frozen(true);
  nigori.set_keystore_migration_time(1U);
  nigori.set_encrypt_everything(true);
  nigori.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
  nigori.set_custom_passphrase_key_derivation_method(
      EnumKeyDerivationMethodToProto(method));

  std::string encoded_salt;
  switch (method) {
    case syncer::KeyDerivationMethod::PBKDF2_HMAC_SHA1_1003:
      // Nothing to do; no further information needs to be extracted from
      // Nigori.
      break;
    case syncer::KeyDerivationMethod::SCRYPT_8192_8_11:
      base::Base64Encode(passphrase_key_params.derivation_params.scrypt_salt(),
                         &encoded_salt);
      nigori.set_custom_passphrase_key_derivation_salt(encoded_salt);
      break;
    case syncer::KeyDerivationMethod::UNSUPPORTED:
      ADD_FAILURE()
          << "Unsupported method in KeyParams, cannot construct Nigori.";
      break;
  }

  // Create the cryptographer, which encrypts with the key derived from
  // |passphrase_key_params| and can decrypt with the key derived from
  // |old_key_params| if given. |encryption_keybag| is a serialized version
  // of this cryptographer |key_bag| encrypted with its encryption key.
  auto cryptographer = syncer::CryptographerImpl::FromSingleKeyForTesting(
      passphrase_key_params.password, passphrase_key_params.derivation_params);
  if (old_key_params) {
    cryptographer->EmplaceKey(old_key_params->password,
                              old_key_params->derivation_params);
  }
  sync_pb::CryptographerData proto = cryptographer->ToProto();
  bool encrypt_result = cryptographer->Encrypt(
      proto.key_bag(), nigori.mutable_encryption_keybag());
  DCHECK(encrypt_result);

  return nigori;
}

sync_pb::EntitySpecifics GetEncryptedBookmarkEntitySpecifics(
    const sync_pb::BookmarkSpecifics& bookmark_specifics,
    const KeyParams& key_params) {
  sync_pb::EntitySpecifics new_specifics;

  sync_pb::EntitySpecifics wrapped_entity_specifics;
  *wrapped_entity_specifics.mutable_bookmark() = bookmark_specifics;
  auto cryptographer = syncer::CryptographerImpl::FromSingleKeyForTesting(
      key_params.password, key_params.derivation_params);

  bool encrypt_result = cryptographer->Encrypt(
      wrapped_entity_specifics, new_specifics.mutable_encrypted());
  DCHECK(encrypt_result);

  new_specifics.mutable_bookmark()->set_legacy_canonicalized_title("encrypted");
  new_specifics.mutable_bookmark()->set_url("encrypted");

  return new_specifics;
}

void SetNigoriInFakeServer(fake_server::FakeServer* fake_server,
                           const sync_pb::NigoriSpecifics& nigori) {
  std::string nigori_entity_id =
      fake_server->GetTopLevelPermanentItemId(syncer::NIGORI);
  ASSERT_NE(nigori_entity_id, "");
  sync_pb::EntitySpecifics nigori_entity_specifics;
  *nigori_entity_specifics.mutable_nigori() = nigori;
  fake_server->ModifyEntitySpecifics(nigori_entity_id, nigori_entity_specifics);
}

void SetKeystoreNigoriInFakeServer(fake_server::FakeServer* fake_server) {
  const std::vector<std::vector<uint8_t>>& keystore_keys =
      fake_server->GetKeystoreKeys();
  ASSERT_EQ(keystore_keys.size(), 1u);
  const std::string passphrase = base::Base64Encode(keystore_keys.back());
  SetNigoriInFakeServer(fake_server, BuildKeystoreNigoriSpecifics(passphrase));
}

}  // namespace encryption_helper

ServerNigoriChecker::ServerNigoriChecker(
    syncer::ProfileSyncService* service,
    fake_server::FakeServer* fake_server,
    syncer::PassphraseType expected_passphrase_type)
    : SingleClientStatusChangeChecker(service),
      fake_server_(fake_server),
      expected_passphrase_type_(expected_passphrase_type) {}

bool ServerNigoriChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for a Nigori node with the proper passphrase type to become "
         "available on the server.";

  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server_->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  EXPECT_LE(nigori_entities.size(), 1U);
  return !nigori_entities.empty() &&
         syncer::ProtoPassphraseInt32ToEnum(
             nigori_entities[0].specifics().nigori().passphrase_type()) ==
             expected_passphrase_type_;
}

ServerNigoriKeyNameChecker::ServerNigoriKeyNameChecker(
    const std::string& expected_key_name,
    syncer::ProfileSyncService* service,
    fake_server::FakeServer* fake_server)
    : SingleClientStatusChangeChecker(service),
      fake_server_(fake_server),
      expected_key_name_(expected_key_name) {}

bool ServerNigoriKeyNameChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::SyncEntity> nigori_entities =
      fake_server_->GetPermanentSyncEntitiesByModelType(syncer::NIGORI);
  DCHECK_EQ(nigori_entities.size(), 1U);

  const std::string given_key_name =
      nigori_entities[0].specifics().nigori().encryption_keybag().key_name();

  *os << "Waiting for a Nigori node with proper key bag encryption key name ("
      << expected_key_name_ << ") to become available on the server."
      << "The server key bag encryption key name is " << given_key_name << ".";
  return given_key_name == expected_key_name_;
}

PassphraseRequiredStateChecker::PassphraseRequiredStateChecker(
    syncer::ProfileSyncService* service,
    bool desired_state)
    : SingleClientStatusChangeChecker(service), desired_state_(desired_state) {}

bool PassphraseRequiredStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting until decryption passphrase is " +
             std::string(desired_state_ ? "required" : "not required");
  return service()
             ->GetUserSettings()
             ->IsPassphraseRequiredForPreferredDataTypes() == desired_state_;
}

TrustedVaultKeyRequiredStateChecker::TrustedVaultKeyRequiredStateChecker(
    syncer::ProfileSyncService* service,
    bool desired_state)
    : SingleClientStatusChangeChecker(service), desired_state_(desired_state) {}

bool TrustedVaultKeyRequiredStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting until trusted vault keys are " +
             std::string(desired_state_ ? "required" : "not required");
  return service()
             ->GetUserSettings()
             ->IsTrustedVaultKeyRequiredForPreferredDataTypes() ==
         desired_state_;
}

TrustedVaultKeysChangedStateChecker::TrustedVaultKeysChangedStateChecker(
    syncer::ProfileSyncService* service)
    : keys_changed_(false) {
  // base::Unretained() is safe here, because callback won't be called once
  // |subscription_| is destroyed.
  subscription_ = service->GetSyncClientForTest()
                      ->GetTrustedVaultClient()
                      ->AddKeysChangedObserver(base::BindRepeating(
                          &TrustedVaultKeysChangedStateChecker::OnKeysChanged,
                          base::Unretained(this)));
}

TrustedVaultKeysChangedStateChecker::~TrustedVaultKeysChangedStateChecker() =
    default;

bool TrustedVaultKeysChangedStateChecker::IsExitConditionSatisfied(
    std::ostream* os) {
  *os << "Waiting for trusted vault keys change";
  return keys_changed_;
}

void TrustedVaultKeysChangedStateChecker::OnKeysChanged() {
  keys_changed_ = true;
}

ScopedScryptFeatureToggler::ScopedScryptFeatureToggler(
    bool force_disabled,
    bool use_for_new_passphrases) {
  std::vector<base::Feature> enabled_features;
  std::vector<base::Feature> disabled_features;
  if (force_disabled) {
    enabled_features.push_back(
        switches::kSyncForceDisableScryptForCustomPassphrase);
  } else {
    disabled_features.push_back(
        switches::kSyncForceDisableScryptForCustomPassphrase);
  }
  if (use_for_new_passphrases) {
    enabled_features.push_back(switches::kSyncUseScryptForNewCustomPassphrases);
  } else {
    disabled_features.push_back(
        switches::kSyncUseScryptForNewCustomPassphrases);
  }
  feature_list_.InitWithFeatures(enabled_features, disabled_features);
}
