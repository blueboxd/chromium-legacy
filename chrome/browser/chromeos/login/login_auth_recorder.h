// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_AUTH_RECORDER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_AUTH_RECORDER_H_

#include "base/macros.h"
#include "base/optional.h"
#include "components/session_manager/core/session_manager_observer.h"

namespace chromeos {

// A metrics recorder that records login authentication related metrics.
// This keeps track of the last authentication method we used and records
// switching between different authentication methods.
// This is tied to LoginScreenClient lifetime.
class LoginAuthRecorder : public session_manager::SessionManagerObserver {
 public:
  // Authentication method to unlock the screen. This enum is used to back an
  // UMA histogram and new values should be inserted immediately above
  // kMethodCount.
  enum class AuthMethod {
    kPassword = 0,
    kPin,
    kSmartlock,
    kFingerprint,
    kChallengeResponse,
    kMethodCount,
  };

  // The type of switching between auth methods. This enum is used to back an
  // UMA histogram and new values should be inserted immediately above
  // kSwitchTypeCount.
  enum class AuthMethodSwitchType {
    kPasswordToPin = 0,
    kPasswordToSmartlock,
    kPinToPassword,
    kPinToSmartlock,
    kSmartlockToPassword,
    kSmartlockToPin,
    kPasswordToFingerprint,
    kPinToFingerprint,
    kSmartlockToFingerprint,
    kFingerprintToPassword,
    kFingerprintToPin,
    kFingerprintToSmartlock,
    kPasswordToChallengeResponse,
    kSwitchTypeCount,
  };

  LoginAuthRecorder();
  ~LoginAuthRecorder() override;

  // Called when user attempts authentication using AuthMethod |type|.
  void RecordAuthMethod(AuthMethod type);

  // Called after a fingerprint attempt to record the auth result.
  // |num_attempts|:  Only valid when auth success to record number of attempts.
  void RecordFingerprintAuthSuccess(bool success,
                                    const base::Optional<int>& num_attempts);

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

 private:
  AuthMethod last_auth_method_ = AuthMethod::kPassword;

  DISALLOW_COPY_AND_ASSIGN(LoginAuthRecorder);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_LOGIN_AUTH_RECORDER_H_
