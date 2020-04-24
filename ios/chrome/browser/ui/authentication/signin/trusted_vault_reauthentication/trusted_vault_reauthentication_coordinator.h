// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_TRUSTED_VAULT_REAUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_TRUSTED_VAULT_REAUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_H_

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

// Coordinates the Trusted Vault re-authentication dialog. Trusted Valut is
// managed by IOSTrustedValueClient.
@interface TrustedVaultReauthenticationCoordinator : SigninCoordinator

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_SIGNIN_TRUSTED_VAULT_REAUTHENTICATION_TRUSTED_VAULT_REAUTHENTICATION_COORDINATOR_H_
