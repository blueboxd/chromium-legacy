// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_DM_STORAGE_H_
#define CHROME_UPDATER_DM_STORAGE_H_

#include <memory>
#include <string>
#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "build/build_config.h"

namespace updater {

class CachedPolicyInfo;
class PolicyManagerInterface;

// DM policy map: policy_type --> serialized_policy_data
using DMPolicyMap = base::flat_map<std::string, std::string>;

// The token service interface defines how to serialize tokens.
class TokenServiceInterface {
 public:
  virtual ~TokenServiceInterface() = default;

  // ID of the device that the tokens target to.
  virtual std::string GetDeviceID() = 0;

  // Writes |enrollment_token| to storage.
  virtual bool StoreEnrollmentToken(const std::string& enrollment_token) = 0;

  // Reads the enrollment token from sources as-needed to find one.
  // Returns an empty string if no enrollment token is found.
  virtual std::string GetEnrollmentToken() = 0;

  // Writes |dm_token| into storage.
  virtual bool StoreDmToken(const std::string& dm_token) = 0;

  // Returns the device management token from storage, or returns an empty
  // string if no device management token is found.
  virtual std::string GetDmToken() = 0;
};

// The DMStorage is responsible for serialization of:
//   1) DM enrollment token.
//   2) DM token.
//   3) DM policies.
class DMStorage {
 public:
  explicit DMStorage(const base::FilePath& policy_cache_root);
  DMStorage(const base::FilePath& policy_cache_root,
            std::unique_ptr<TokenServiceInterface> token_service);
  DMStorage(const DMStorage&) = delete;
  DMStorage& operator=(const DMStorage&) = delete;
  virtual ~DMStorage();

  // Forwards to token service to get device ID
  std::string GetDeviceID() { return token_service_->GetDeviceID(); }

  // Forwards to token service to save enrollment token.
  bool StoreEnrollmentToken(const std::string& enrollment_token) {
    return token_service_->StoreEnrollmentToken(enrollment_token);
  }

  // Forwards to token service to get enrollment token.
  std::string GetEnrollmentToken() {
    return token_service_->GetEnrollmentToken();
  }

  // Forwards to token service to save DM token.
  bool StoreDmToken(const std::string& dm_token) {
    return token_service_->StoreDmToken(dm_token);
  }

  // Forwards to token service to get DM token.
  std::string GetDmToken() { return token_service_->GetDmToken(); }

  // Writes a special DM token to storage to mark current device as
  // deregistered.
  bool DeregisterDevice();

  // Returns true if the DM token is valid, where valid is defined as non-blank
  // and not de-registered.
  bool IsValidDMToken();

  // Persists DM policies.
  //
  // |policy_info_data| is the serialized data of a PolicyFetchResponse. It will
  // be saved into a fixed file named "CachedPolicyInfo" in cache root. The
  // file content will be used to construct an updater::CachedPolicyInfo object
  // to get public key, its version, and signing timestamp. The values will
  // be used in subsequent policy fetches.
  //
  // Each entry in |policy_map| will be stored within a sub-directory named
  // {Base64Encoded{policy_type}}, with a fixed file name of
  // "PolicyFetchResponse", where the file contents are serialized data of
  // the policy object.
  //
  // Please note that this function also purges all stale polices whose policy
  // type does not appear in keys of |policies|.
  //
  // Visualized directory structure example:
  //  <policy_cache_root_>
  //   |-- CachedPolicyInfo                      # Policy meta-data file.
  //   |-- Z29vZ2xlL21hY2hpbmUtbGV2ZWwtb21haGE=
  //   |       `--PolicyFetchResponse            # Policy response data.
  //   `-- Zm9vYmFy                              # b64("foobar").
  //           `--PolicyFetchResponse            # Policy response data.
  //
  //  ('Z29vZ2xlL21hY2hpbmUtbGV2ZWwtb21haGE=' is base64 encoding of
  //  "google/machine-level-omaha").
  //
  bool PersistPolicies(const std::string& policy_info_data,
                       const DMPolicyMap& policy_map) const;

  // Creates a CachedPolicyInfo object and populates it with the public key
  // information loaded from file |policy_cache_root_|\CachedPolicyInfo.
  std::unique_ptr<CachedPolicyInfo> GetCachedPolicyInfo();

  // Creates a policy manager and populates it with the Omaha policies loaded
  // from PolicyFetchResponse file within
  // |policy_cache_root_|\{Base64Encoded{kGoogleUpdatePolicyType}} directory.
  std::unique_ptr<PolicyManagerInterface> GetOmahaPolicyManager();

 private:
  const base::FilePath policy_cache_root_;
  std::unique_ptr<TokenServiceInterface> token_service_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_DM_STORAGE_H_
