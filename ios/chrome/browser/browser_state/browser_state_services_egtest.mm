// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#include "base/bind.h"
#include "base/run_loop.h"
#import "base/test/ios/wait_util.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/web/public/browser_state.h"
#include "ios/web/public/service/service_manager_connection.h"
#include "services/identity/public/mojom/constants.mojom.h"
#include "services/identity/public/mojom/identity_accessor.mojom.h"
#include "services/service_manager/public/cpp/connector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Callback passed to
// identity::mojom::IdentityAccessor::GetPrimaryAccountInfo(). Sets
// |echo_callback_called_flag| to true to indicate that the callback was
// invoked.
void OnGotPrimaryAccountInfo(
    bool* get_primary_account_info_callback_called_flag,
    const base::Optional<CoreAccountId>& account_id,
    const base::Optional<std::string>& gaia,
    const base::Optional<std::string>& email,
    const identity::AccountState& account_state) {
  GREYAssert(!account_id, @"AccountId has unexpected value");
  *get_primary_account_info_callback_called_flag = true;
}

// Waits until a given callback is invoked (as signalled by that callback
// setting |callback_called_flag| to true).
void WaitForCallback(const std::string& callback_name,
                     bool* callback_called_flag) {
  GREYCondition* condition =
      [GREYCondition conditionWithName:@"Wait for callback"
                                 block:^BOOL {
                                   return *callback_called_flag;
                                 }];
  GREYAssert(
      [condition waitWithTimeout:base::test::ios::kWaitForUIElementTimeout],
      @"Failed waiting for %s callback", callback_name.c_str());
}
}

// Tests embedding of various services in ChromeBrowserState.
@interface BrowserStateServicesTestCase : ChromeTestCase
@end

@implementation BrowserStateServicesTestCase

// Tests that it is possible to connect to the Identity Service.
- (void)testConnectionToIdentityService {
  ios::ChromeBrowserState* browserState =
      chrome_test_util::GetOriginalBrowserState();

  // Connect to the Identity Service and bind an IdentityManager instance.
  identity::mojom::IdentityAccessorPtr identityAccessor;
  web::BrowserState::GetConnectorFor(browserState)
      ->BindInterface(identity::mojom::kServiceName,
                      mojo::MakeRequest(&identityAccessor));

  bool getPrimaryAccountInfoCallbackCalled = false;
  identityAccessor->GetPrimaryAccountInfo(base::BindOnce(
      &OnGotPrimaryAccountInfo, &getPrimaryAccountInfoCallbackCalled));

  WaitForCallback("GetPrimaryAccountInfo",
                  &getPrimaryAccountInfoCallbackCalled);
}

@end
