// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/test/fake_sync_encryption_handler.h"

#include "base/base64.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/passphrase_enums.h"
#include "components/sync/protocol/nigori_specifics.pb.h"
#include "components/sync/syncable/nigori_util.h"

namespace syncer {

FakeSyncEncryptionHandler::FakeSyncEncryptionHandler()
    : encrypted_types_(AlwaysEncryptedUserTypes()),
      encrypt_everything_(false),
      passphrase_type_(PassphraseType::kImplicitPassphrase),
      cryptographer_(CryptographerImpl::CreateEmpty()) {}

FakeSyncEncryptionHandler::~FakeSyncEncryptionHandler() = default;

bool FakeSyncEncryptionHandler::Init() {
  // Set up a basic cryptographer.
  const std::string keystore_key = "keystore_key";
  cryptographer_->EmplaceKey(keystore_key,
                             KeyDerivationParams::CreateForPbkdf2());
  return true;
}

bool FakeSyncEncryptionHandler::ApplyNigoriUpdate(
    const sync_pb::NigoriSpecifics& nigori,
    syncable::BaseTransaction* const trans) {
  return false;
}

void FakeSyncEncryptionHandler::UpdateNigoriFromEncryptedTypes(
    sync_pb::NigoriSpecifics* nigori,
    const syncable::BaseTransaction* const trans) const {
  syncable::UpdateNigoriFromEncryptedTypes(encrypted_types_,
                                           encrypt_everything_, nigori);
}

bool FakeSyncEncryptionHandler::NeedKeystoreKey() const {
  return keystore_key_.empty();
}

bool FakeSyncEncryptionHandler::SetKeystoreKeys(
    const std::vector<std::vector<uint8_t>>& keys) {
  if (keys.empty())
    return false;
  std::vector<uint8_t> new_key = keys.back();
  if (new_key.empty())
    return false;
  keystore_key_ = new_key;

  DVLOG(1) << "Keystore bootstrap token updated.";
  for (auto& observer : observers_)
    observer.OnBootstrapTokenUpdated(base::Base64Encode(keystore_key_),
                                     KEYSTORE_BOOTSTRAP_TOKEN);

  return true;
}

const Cryptographer* FakeSyncEncryptionHandler::GetCryptographer(
    const syncable::BaseTransaction* const trans) const {
  return cryptographer_.get();
}

ModelTypeSet FakeSyncEncryptionHandler::GetEncryptedTypes(
    const syncable::BaseTransaction* const trans) const {
  return encrypted_types_;
}

void FakeSyncEncryptionHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSyncEncryptionHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FakeSyncEncryptionHandler::SetEncryptionPassphrase(
    const std::string& passphrase) {
  passphrase_type_ = PassphraseType::kCustomPassphrase;
}

void FakeSyncEncryptionHandler::SetDecryptionPassphrase(
    const std::string& passphrase) {
  // Do nothing.
}

void FakeSyncEncryptionHandler::AddTrustedVaultDecryptionKeys(
    const std::vector<std::vector<uint8_t>>& encryption_keys) {
  // Do nothing.
}

void FakeSyncEncryptionHandler::EnableEncryptEverything() {
  if (encrypt_everything_)
    return;
  encrypt_everything_ = true;
  encrypted_types_ = ModelTypeSet::All();
  for (auto& observer : observers_)
    observer.OnEncryptedTypesChanged(encrypted_types_, encrypt_everything_);
}

bool FakeSyncEncryptionHandler::IsEncryptEverythingEnabled() const {
  return encrypt_everything_;
}

PassphraseType FakeSyncEncryptionHandler::GetPassphraseType(
    const syncable::BaseTransaction* const trans) const {
  return passphrase_type_;
}

base::Time FakeSyncEncryptionHandler::GetKeystoreMigrationTime() const {
  return base::Time();
}

KeystoreKeysHandler* FakeSyncEncryptionHandler::GetKeystoreKeysHandler() {
  return this;
}

}  // namespace syncer
