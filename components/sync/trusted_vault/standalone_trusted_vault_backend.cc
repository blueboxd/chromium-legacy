// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/trusted_vault/standalone_trusted_vault_backend.h"

#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "components/os_crypt/os_crypt.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace syncer {

namespace {

sync_pb::LocalTrustedVault ReadEncryptedFile(const base::FilePath& file_path) {
  sync_pb::LocalTrustedVault proto;
  std::string ciphertext;
  std::string decrypted_content;
  if (base::ReadFileToString(file_path, &ciphertext) &&
      OSCrypt::DecryptString(ciphertext, &decrypted_content)) {
    proto.ParseFromString(decrypted_content);
  }

  return proto;
}

void WriteToDisk(const sync_pb::LocalTrustedVault& data,
                 const base::FilePath& file_path) {
  std::string encrypted_data;
  if (!OSCrypt::EncryptString(data.SerializeAsString(), &encrypted_data)) {
    DLOG(ERROR) << "Failed to encrypt trusted vault file.";
    return;
  }

  if (!base::ImportantFileWriter::WriteFileAtomically(file_path,
                                                      encrypted_data)) {
    DLOG(ERROR) << "Failed to write trusted vault file.";
  }
}

}  // namespace

StandaloneTrustedVaultBackend::StandaloneTrustedVaultBackend(
    const base::FilePath& file_path)
    : file_path_(file_path) {}

void StandaloneTrustedVaultBackend::ReadDataFromDisk() {
  data_ = ReadEncryptedFile(file_path_);
}

std::vector<std::vector<uint8_t>> StandaloneTrustedVaultBackend::FetchKeys(
    const CoreAccountInfo& account_info) {
  const sync_pb::LocalTrustedVaultPerUser* per_user_vault =
      FindUserVault(account_info.gaia);

  std::vector<std::vector<uint8_t>> keys;
  if (per_user_vault) {
    for (const sync_pb::LocalTrustedVaultKey& key : per_user_vault->key()) {
      const std::string& key_material = key.key_material();
      keys.emplace_back(key_material.begin(), key_material.end());
    }
  }

  return keys;
}

void StandaloneTrustedVaultBackend::StoreKeys(
    const std::string& gaia_id,
    const std::vector<std::vector<uint8_t>>& keys,
    int last_key_version) {
  // Find or create user for |gaid_id|.
  sync_pb::LocalTrustedVaultPerUser* per_user_vault = FindUserVault(gaia_id);
  if (!per_user_vault) {
    per_user_vault = data_.add_user();
    per_user_vault->set_gaia_id(gaia_id);
  }

  // Replace all keys.
  per_user_vault->set_last_key_version(last_key_version);
  per_user_vault->clear_key();
  for (const std::vector<uint8_t>& key : keys) {
    per_user_vault->add_key()->set_key_material(key.data(), key.size());
  }

  WriteToDisk(data_, file_path_);
}

void StandaloneTrustedVaultBackend::RemoveAllStoredKeys() {
  base::DeleteFile(file_path_);
  data_.Clear();
}

sync_pb::LocalTrustedVaultPerUser* StandaloneTrustedVaultBackend::FindUserVault(
    const std::string& gaia_id) {
  for (int i = 0; i < data_.user_size(); ++i) {
    if (data_.user(i).gaia_id() == gaia_id) {
      return data_.mutable_user(i);
    }
  }
  return nullptr;
}

}  // namespace syncer
