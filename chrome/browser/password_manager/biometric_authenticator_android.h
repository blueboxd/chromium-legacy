// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATOR_ANDROID_H_

#include "chrome/browser/password_manager/chrome_biometric_authenticator.h"
#include "components/password_manager/core/browser/biometric_authenticator.h"
#include "components/password_manager/core/browser/origin_credential_store.h"

// Android implementation of the BiometricAuthenticator interface.
class BiometicAuthenticatorAndroid : public ChromeBiometricAuthenticator {
 public:
  password_manager::BiometricsAvailability CanAuthenticate() override;
  void Authenticate(const password_manager::UiCredential& credential,
                    AuthenticateCallback callback) override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_BIOMETRIC_AUTHENTICATOR_ANDROID_H_
