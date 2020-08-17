// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/protocol/local_trusted_vault.pb.h"
#include "components/sync/trusted_vault/trusted_vault_connection.h"

namespace syncer {

// Provides interfaces to store/remove keys to/from file storage.
// This class performs expensive operations and expected to be run from
// dedicated sequence (using thread pool). Can be constructed on any thread/
// sequence.
class StandaloneTrustedVaultBackend
    : public base::RefCountedThreadSafe<StandaloneTrustedVaultBackend> {
 public:
  StandaloneTrustedVaultBackend(
      const base::FilePath& file_path,
      std::unique_ptr<TrustedVaultConnection> connection);
  StandaloneTrustedVaultBackend(const StandaloneTrustedVaultBackend& other) =
      delete;
  StandaloneTrustedVaultBackend& operator=(
      const StandaloneTrustedVaultBackend& other) = delete;

  // Restores state saved in |file_path_|, should be called before using the
  // object.
  void ReadDataFromDisk();

  // Returns keys corresponding to |account_info|.
  std::vector<std::vector<uint8_t>> FetchKeys(
      const CoreAccountInfo& account_info);

  // Replaces keys for given |gaia_id| both in memory and in |file_path_|.
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::vector<uint8_t>>& keys,
                 int last_key_version);

  // Removes all keys for all accounts from both memory and |file_path_|.
  void RemoveAllStoredKeys();

  // Sets/resets |syncing_account_|.
  void SetSyncingAccount(
      const base::Optional<CoreAccountInfo>& syncing_account);

  sync_pb::LocalDeviceRegistrationInfo GetDeviceRegistrationInfoForTesting(
      const std::string& gaia_id);

 private:
  friend class base::RefCountedThreadSafe<StandaloneTrustedVaultBackend>;

  ~StandaloneTrustedVaultBackend();

  // Finds the per-user vault in |data_| for |gaia_id|. Returns null if not
  // found.
  sync_pb::LocalTrustedVaultPerUser* FindUserVault(const std::string& gaia_id);

  // Attempts to register device in case it's not yet registered and currently
  // available local data is sufficient to do it.
  void MaybeRegisterDevice(const std::string& gaia_id);

  // Called when device registration for |gaia_id| is completed (either
  // successfully or not).
  void OnDeviceRegistered(const std::string& gaia_id,
                          TrustedVaultRequestStatus status);

  const base::FilePath file_path_;

  sync_pb::LocalTrustedVault data_;

  // Only current |syncing_account_| can be used for communication with trusted
  // vault server.
  base::Optional<CoreAccountInfo> syncing_account_;

  // Used for communication with trusted vault server.
  std::unique_ptr<TrustedVaultConnection> connection_;

  // Used for cancellation of callbacks passed to |connection_|.
  base::WeakPtrFactory<StandaloneTrustedVaultBackend>
      weak_factory_for_connection_{this};
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_
