// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_DM_CACHED_POLICY_INFO_H_
#define CHROME_UPDATER_DM_CACHED_POLICY_INFO_H_

#include <string>

namespace updater {

class CachedPolicyInfo {
 public:
  CachedPolicyInfo();
  CachedPolicyInfo(const CachedPolicyInfo&) = delete;
  CachedPolicyInfo& operator=(const CachedPolicyInfo& other) = delete;
  ~CachedPolicyInfo();

  // Populate members with serialized data of DM PolicyFetchResponse.
  bool Populate(const std::string& raw_response);

  // Public key of the policy.
  std::string PublicKey() const { return key_; }

  // Version of the public key. -1 means the key is not versioned or unknown.
  int32_t KeyVersion() const { return key_version_; }

  bool HasKeyVersion() const { return key_version_ >= 0; }

  // Signing timestamp.
  int64_t TimeStamp() const { return timestamp_; }

 private:
  std::string key_;
  int32_t key_version_;
  int64_t timestamp_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_DM_CACHED_POLICY_INFO_H_
