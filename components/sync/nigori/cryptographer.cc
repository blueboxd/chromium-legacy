// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/nigori/cryptographer.h"

#include <stddef.h>

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/logging.h"
#include "components/sync/base/encryptor.h"
#include "components/sync/protocol/nigori_specifics.pb.h"

namespace syncer {

KeyParams::KeyParams(KeyDerivationParams derivation_params,
                     const std::string& password)
    : derivation_params(derivation_params), password(password) {}

KeyParams::KeyParams(const KeyParams& other) = default;
KeyParams::KeyParams(KeyParams&& other) = default;
KeyParams::~KeyParams() = default;

Cryptographer::Cryptographer() : key_bag_(NigoriKeyBag::CreateEmpty()) {}

Cryptographer::Cryptographer(const Cryptographer& other)
    : key_bag_(other.key_bag_.Clone()),
      default_nigori_name_(other.default_nigori_name_) {
  if (other.pending_keys_) {
    pending_keys_ =
        std::make_unique<sync_pb::EncryptedData>(*(other.pending_keys_));
  }
}

Cryptographer::~Cryptographer() {}

void Cryptographer::Bootstrap(const Encryptor& encryptor,
                              const std::string& restored_bootstrap_token) {
  if (is_initialized()) {
    NOTREACHED();
    return;
  }

  std::string serialized_nigori_key =
      UnpackBootstrapToken(encryptor, restored_bootstrap_token);
  if (serialized_nigori_key.empty())
    return;
  ImportNigoriKey(serialized_nigori_key);
}

bool Cryptographer::CanDecrypt(const sync_pb::EncryptedData& data) const {
  return key_bag_.HasKey(data.key_name());
}

bool Cryptographer::CanDecryptUsingDefaultKey(
    const sync_pb::EncryptedData& data) const {
  return !default_nigori_name_.empty() &&
         data.key_name() == default_nigori_name_;
}

bool Cryptographer::Encrypt(const ::google::protobuf::MessageLite& message,
                            sync_pb::EncryptedData* encrypted) const {
  DCHECK(encrypted);
  if (default_nigori_name_.empty()) {
    LOG(ERROR) << "Cryptographer not ready, failed to encrypt.";
    return false;
  }

  std::string serialized;
  if (!message.SerializeToString(&serialized)) {
    LOG(ERROR) << "Message is invalid/missing a required field.";
    return false;
  }

  return EncryptString(serialized, encrypted);
}

bool Cryptographer::EncryptString(const std::string& serialized,
                                  sync_pb::EncryptedData* encrypted) const {
  if (CanDecryptUsingDefaultKey(*encrypted)) {
    std::string original_serialized;
    if (DecryptToString(*encrypted, &original_serialized) &&
        original_serialized == serialized) {
      DVLOG(2) << "Re-encryption unnecessary, encrypted data already matches.";
      return true;
    }
  }

  if (!key_bag_.HasKey(default_nigori_name_)) {
    LOG(ERROR) << "Corrupt default key.";
    return false;
  }

  return key_bag_.EncryptWithKey(default_nigori_name_, serialized, encrypted);
}

bool Cryptographer::Decrypt(const sync_pb::EncryptedData& encrypted,
                            ::google::protobuf::MessageLite* message) const {
  DCHECK(message);
  std::string plaintext;
  if (!DecryptToString(encrypted, &plaintext)) {
    return false;
  }
  return message->ParseFromString(plaintext);
}

bool Cryptographer::DecryptToString(const sync_pb::EncryptedData& encrypted,
                                    std::string* decrypted) const {
  return key_bag_.Decrypt(encrypted, decrypted);
}

bool Cryptographer::GetKeys(sync_pb::EncryptedData* encrypted) const {
  DCHECK(encrypted);
  DCHECK_NE(size_t(0), key_bag_.size());

  // Create a bag of all the Nigori parameters we know about.
  sync_pb::NigoriKeyBag bag = key_bag_.ToProto();

  // Encrypt the bag with the default Nigori.
  return Encrypt(bag, encrypted);
}

bool Cryptographer::AddKey(const KeyParams& params) {
  return AddKeyImpl(
      Nigori::CreateByDerivation(params.derivation_params, params.password),
      /*set_as_default=*/true);
}

bool Cryptographer::AddNonDefaultKey(const KeyParams& params) {
  DCHECK(is_initialized());
  return AddKeyImpl(
      Nigori::CreateByDerivation(params.derivation_params, params.password),
      /*set_as_default=*/false);
}

bool Cryptographer::AddKeyFromBootstrapToken(
    const Encryptor& encryptor,
    const std::string& restored_bootstrap_token) {
  // Create the new Nigori and make it the default encryptor.
  std::string serialized_nigori_key =
      UnpackBootstrapToken(encryptor, restored_bootstrap_token);
  return ImportNigoriKey(serialized_nigori_key);
}

bool Cryptographer::AddKeyImpl(std::unique_ptr<Nigori> nigori,
                               bool set_as_default) {
  DCHECK(nigori);
  std::string key_name = key_bag_.AddKey(std::move(nigori));
  if (key_name.empty()) {
    NOTREACHED();
    return false;
  }

  // Check if the key we just added can decrypt the pending keys and add them
  // too if so.
  if (pending_keys_.get() && CanDecrypt(*pending_keys_)) {
    sync_pb::NigoriKeyBag pending_bag;
    Decrypt(*pending_keys_, &pending_bag);
    InstallKeyBag(pending_bag);
    SetDefaultKey(pending_keys_->key_name());
    pending_keys_.reset();
  }

  // The just-added key takes priority over the pending keys as default.
  if (set_as_default)
    SetDefaultKey(key_name);
  return true;
}

void Cryptographer::InstallKeys(const sync_pb::EncryptedData& encrypted) {
  DCHECK(CanDecrypt(encrypted));

  sync_pb::NigoriKeyBag bag;
  if (!Decrypt(encrypted, &bag))
    return;
  InstallKeyBag(bag);
}

void Cryptographer::SetDefaultKey(const std::string& key_name) {
  DCHECK(key_bag_.HasKey(key_name));
  default_nigori_name_ = key_name;
}

bool Cryptographer::is_initialized() const {
  return !default_nigori_name_.empty();
}

void Cryptographer::SetPendingKeys(const sync_pb::EncryptedData& encrypted) {
  DCHECK(!CanDecrypt(encrypted));
  DCHECK(!encrypted.blob().empty());
  pending_keys_ = std::make_unique<sync_pb::EncryptedData>(encrypted);
}

const sync_pb::EncryptedData& Cryptographer::GetPendingKeys() const {
  DCHECK(has_pending_keys());
  return *(pending_keys_.get());
}

bool Cryptographer::DecryptPendingKeys(const KeyParams& params) {
  DCHECK_NE(KeyDerivationMethod::UNSUPPORTED,
            params.derivation_params.method());

  std::unique_ptr<Nigori> nigori =
      Nigori::CreateByDerivation(params.derivation_params, params.password);

  std::string plaintext;
  if (!nigori->Decrypt(pending_keys_->blob(), &plaintext))
    return false;

  sync_pb::NigoriKeyBag bag;
  if (!bag.ParseFromString(plaintext)) {
    NOTREACHED();
    return false;
  }
  InstallKeyBag(bag);
  const std::string& new_default_key_name = pending_keys_->key_name();
  SetDefaultKey(new_default_key_name);
  pending_keys_.reset();
  return true;
}

bool Cryptographer::GetBootstrapToken(const Encryptor& encryptor,
                                      std::string* token) const {
  DCHECK(token);
  std::string unencrypted_token = GetDefaultNigoriKeyData();
  if (unencrypted_token.empty())
    return false;

  std::string encrypted_token;
  if (!encryptor.EncryptString(unencrypted_token, &encrypted_token)) {
    return false;
  }

  base::Base64Encode(encrypted_token, token);

  return true;
}

std::string Cryptographer::UnpackBootstrapToken(
    const Encryptor& encryptor,
    const std::string& token) const {
  if (token.empty())
    return std::string();

  std::string encrypted_data;
  if (!base::Base64Decode(token, &encrypted_data)) {
    DLOG(WARNING) << "Could not decode token.";
    return std::string();
  }

  std::string unencrypted_token;
  if (!encryptor.DecryptString(encrypted_data, &unencrypted_token)) {
    DLOG(WARNING) << "Decryption of bootstrap token failed.";
    return std::string();
  }
  return unencrypted_token;
}

void Cryptographer::InstallKeyBag(const sync_pb::NigoriKeyBag& bag) {
  key_bag_.AddAllUnknownKeysFrom(NigoriKeyBag::CreateFromProto(bag));
}

bool Cryptographer::KeybagIsStale(
    const sync_pb::EncryptedData& encrypted_bag) const {
  if (!is_ready())
    return false;
  if (encrypted_bag.blob().empty())
    return true;
  if (!CanDecrypt(encrypted_bag))
    return false;
  if (!CanDecryptUsingDefaultKey(encrypted_bag))
    return true;
  sync_pb::NigoriKeyBag bag;
  if (!Decrypt(encrypted_bag, &bag)) {
    LOG(ERROR) << "Failed to decrypt keybag for stale check. "
               << "Assuming keybag is corrupted.";
    return true;
  }
  if (static_cast<size_t>(bag.key_size()) < key_bag_.size())
    return true;
  return false;
}

std::string Cryptographer::GetDefaultNigoriKeyName() const {
  return default_nigori_name_;
}

std::string Cryptographer::GetDefaultNigoriKeyData() const {
  if (!is_initialized())
    return std::string();
  sync_pb::NigoriKey key = key_bag_.ExportKey(default_nigori_name_);
  key.clear_name();
  return key.SerializeAsString();
}

bool Cryptographer::ImportNigoriKey(const std::string& serialized_nigori_key) {
  if (serialized_nigori_key.empty())
    return false;

  sync_pb::NigoriKey key;
  if (!key.ParseFromString(serialized_nigori_key))
    return false;

  std::unique_ptr<Nigori> nigori = Nigori::CreateByImport(
      key.user_key(), key.encryption_key(), key.mac_key());

  if (!nigori) {
    DLOG(ERROR) << "Ignoring invalid Nigori when importing";
    return false;
  }

  if (!AddKeyImpl(std::move(nigori), true))
    return false;
  return true;
}

}  // namespace syncer
