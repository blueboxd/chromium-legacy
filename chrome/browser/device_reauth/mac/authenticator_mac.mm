// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_reauth/mac/authenticator_mac.h"

#import <LocalAuthentication/LAContext.h>

#include "base/functional/callback.h"
#include "chrome/browser/password_manager/password_manager_util_mac.h"

AuthenticatorMac::AuthenticatorMac() = default;

AuthenticatorMac::~AuthenticatorMac() = default;

bool AuthenticatorMac::CheckIfBiometricsAvailable() {
  if (@available(macOS 10.12.2, *)) {
    LAContext* context = [[LAContext alloc] init];
    return
        [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthenticationWithBiometrics
                             error:nil];
  } else {
    return NO;
  }
}

bool AuthenticatorMac::CheckIfBiometricsOrScreenLockAvailable() {
  if (@available(macOS 10.11, *)) {
    LAContext* context = [[LAContext alloc] init];

    return [context canEvaluatePolicy:LAPolicyDeviceOwnerAuthentication
                                error:nil];
  } else {
    return NO;
  }
}

bool AuthenticatorMac::AuthenticateUserWithNonBiometrics(
    const std::u16string& message) {
  return password_manager_util_mac::AuthenticateUser(message);
}
