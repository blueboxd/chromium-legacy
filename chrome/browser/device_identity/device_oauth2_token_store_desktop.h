// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_STORE_DESKTOP_H_
#define CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_STORE_DESKTOP_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/device_identity/device_oauth2_token_store.h"
#include "google_apis/gaia/core_account_id.h"

class PrefRegistrySimple;
class PrefService;

// The pref name where this class stores the encrypted refresh token.
extern const char kCBCMServiceAccountRefreshToken[];

// The pref name where this class stores the service account's email.
extern const char kCBCMServiceAccountEmail[];

// The desktop (mac, win, linux) implementation of DeviceOAuth2TokenStore. This
// is used by the DeviceOAuth2TokenService on those platforms to encrypt and
// persist the refresh token of the service account to LocalState.
class DeviceOAuth2TokenStoreDesktop : public DeviceOAuth2TokenStore {
 public:
  explicit DeviceOAuth2TokenStoreDesktop(PrefService* local_state);
  ~DeviceOAuth2TokenStoreDesktop() override;

  DeviceOAuth2TokenStoreDesktop(const DeviceOAuth2TokenStoreDesktop& other) =
      delete;
  DeviceOAuth2TokenStoreDesktop& operator=(
      const DeviceOAuth2TokenStoreDesktop& other) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // DeviceOAuth2TokenStore:
  void Init(InitCallback callback) override;
  CoreAccountId GetAccountId() const override;
  std::string GetRefreshToken() const override;
  void SetAndSaveRefreshToken(const std::string& refresh_token,
                              StatusCallback result_callback) override;
  void PrepareTrustedAccountId(TrustedAccountIdCallback callback) override;
  void SetAccountEmail(const std::string& account_email) override;

 private:
  void OnServiceAccountIdentityChanged();

  PrefService* const local_state_;
  std::string refresh_token_;

  base::WeakPtrFactory<DeviceOAuth2TokenStoreDesktop> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_DEVICE_IDENTITY_DEVICE_OAUTH2_TOKEN_STORE_DESKTOP_H_
