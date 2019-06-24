// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_auth_recorder.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

using AuthMethod = LoginAuthRecorder::AuthMethod;
using AuthMethodSwitchType = LoginAuthRecorder::AuthMethodSwitchType;

namespace {

AuthMethodSwitchType SwitchFromPasswordTo(AuthMethod current) {
  DCHECK_NE(AuthMethod::kPassword, current);
  switch (current) {
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kPasswordToPin;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kPasswordToSmartlock;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kPasswordToFingerprint;
    case AuthMethod::kPassword:
    case AuthMethod::kMethodCount:
      NOTREACHED();
      return AuthMethodSwitchType::kSwitchTypeCount;
  }
}

AuthMethodSwitchType SwitchFromPinTo(AuthMethod current) {
  DCHECK_NE(AuthMethod::kPin, current);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kPinToPassword;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kPinToSmartlock;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kPinToFingerprint;
    case AuthMethod::kPin:
    case AuthMethod::kMethodCount:
      NOTREACHED();
      return AuthMethodSwitchType::kSwitchTypeCount;
  }
}

AuthMethodSwitchType SwitchFromSmartlockTo(AuthMethod current) {
  DCHECK_NE(AuthMethod::kSmartlock, current);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kSmartlockToPassword;
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kSmartlockToPin;
    case AuthMethod::kFingerprint:
      return AuthMethodSwitchType::kSmartlockToFingerprint;
    case AuthMethod::kSmartlock:
    case AuthMethod::kMethodCount:
      NOTREACHED();
      return AuthMethodSwitchType::kSwitchTypeCount;
  }
}

AuthMethodSwitchType SwitchFromFingerprintTo(AuthMethod current) {
  DCHECK_NE(AuthMethod::kFingerprint, current);
  switch (current) {
    case AuthMethod::kPassword:
      return AuthMethodSwitchType::kFingerprintToPassword;
    case AuthMethod::kSmartlock:
      return AuthMethodSwitchType::kFingerprintToSmartlock;
    case AuthMethod::kPin:
      return AuthMethodSwitchType::kFingerprintToPin;
    case AuthMethod::kFingerprint:
    case AuthMethod::kMethodCount:
      NOTREACHED();
      return AuthMethodSwitchType::kSwitchTypeCount;
  }
}

AuthMethodSwitchType FindSwitchType(AuthMethod previous, AuthMethod current) {
  DCHECK_NE(previous, current);
  switch (previous) {
    case AuthMethod::kPassword:
      return SwitchFromPasswordTo(current);
    case AuthMethod::kPin:
      return SwitchFromPinTo(current);
    case AuthMethod::kSmartlock:
      return SwitchFromSmartlockTo(current);
    case AuthMethod::kFingerprint:
      return SwitchFromFingerprintTo(current);
    case AuthMethod::kMethodCount:
      NOTREACHED();
      return AuthMethodSwitchType::kSwitchTypeCount;
  }
}

}  // namespace

LoginAuthRecorder::LoginAuthRecorder() {
  session_manager::SessionManager::Get()->AddObserver(this);
}

LoginAuthRecorder::~LoginAuthRecorder() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void LoginAuthRecorder::RecordAuthMethod(AuthMethod method) {
  DCHECK_NE(method, AuthMethod::kMethodCount);
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOCKED) {
    return;
  }

  // Record usage of PIN / Password / Smartlock / Fingerprint in lock screen.
  const bool is_tablet_mode = TabletModeClient::Get()->tablet_mode_enabled();
  if (is_tablet_mode) {
    UMA_HISTOGRAM_ENUMERATION("Ash.Login.Lock.AuthMethod.Used.TabletMode",
                              method, AuthMethod::kMethodCount);
  } else {
    UMA_HISTOGRAM_ENUMERATION("Ash.Login.Lock.AuthMethod.Used.ClamShellMode",
                              method, AuthMethod::kMethodCount);
  }

  if (last_auth_method_ != method) {
    // Record switching between unlock methods.
    UMA_HISTOGRAM_ENUMERATION("Ash.Login.Lock.AuthMethod.Switched",
                              FindSwitchType(last_auth_method_, method),
                              AuthMethodSwitchType::kSwitchTypeCount);

    last_auth_method_ = method;
  }
}

void LoginAuthRecorder::RecordFingerprintAuthSuccess(
    bool success,
    const base::Optional<int>& num_attempts) {
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOCKED) {
    return;
  }

  UMA_HISTOGRAM_BOOLEAN("Fingerprint.Unlock.AuthSuccessful", success);
  if (success) {
    DCHECK(num_attempts);
    UMA_HISTOGRAM_COUNTS_100("Fingerprint.Unlock.AttemptsCountBeforeSuccess",
                             num_attempts.value());
  }
}

void LoginAuthRecorder::OnSessionStateChanged() {
  // Reset local state.
  last_auth_method_ = AuthMethod::kPassword;
}

}  // namespace chromeos
