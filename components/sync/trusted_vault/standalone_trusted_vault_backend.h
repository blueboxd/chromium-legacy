// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_
#define COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_

#include <cstdint>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/sync/protocol/local_trusted_vault.pb.h"

struct CoreAccountInfo;

namespace syncer {

// Provides interfaces to store/remove keys to/from file storage.
// This class performs expensive operations and expected to be run from
// dedicated sequence (using thread pool). Can be constructed on any thread/
// sequence.
class StandaloneTrustedVaultBackend
    : public base::RefCountedThreadSafe<StandaloneTrustedVaultBackend> {
 public:
  explicit StandaloneTrustedVaultBackend(const base::FilePath& file_path);
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

 private:
  friend class base::RefCountedThreadSafe<StandaloneTrustedVaultBackend>;

  ~StandaloneTrustedVaultBackend() = default;

  // Finds the per-user vault in |data_| for |gaia_id|. Returns null if not
  // found.
  sync_pb::LocalTrustedVaultPerUser* FindUserVault(const std::string& gaia_id);

  const base::FilePath file_path_;

  sync_pb::LocalTrustedVault data_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_TRUSTED_VAULT_STANDALONE_TRUSTED_VAULT_BACKEND_H_
