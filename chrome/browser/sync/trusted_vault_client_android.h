// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TRUSTED_VAULT_CLIENT_ANDROID_H_
#define CHROME_BROWSER_SYNC_TRUSTED_VAULT_CLIENT_ANDROID_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "components/sync/driver/trusted_vault_client.h"

// JNI bridge for a Java implementation of the TrustedVaultClient interface,
// used on Android.
//
// This class must be accessed from the UI thread.
class TrustedVaultClientAndroid : public syncer::TrustedVaultClient {
 public:
  TrustedVaultClientAndroid();
  ~TrustedVaultClientAndroid() override;

  TrustedVaultClientAndroid(const TrustedVaultClientAndroid&) = delete;
  TrustedVaultClientAndroid& operator=(const TrustedVaultClientAndroid&) =
      delete;

  // TrustedVaultClient implementation.
  void FetchKeys(
      const std::string& gaia_id,
      base::OnceCallback<void(const std::vector<std::string>&)> cb) override;
  void StoreKeys(const std::string& gaia_id,
                 const std::vector<std::string>& keys) override;
};

#endif  // CHROME_BROWSER_SYNC_TRUSTED_VAULT_CLIENT_ANDROID_H_
