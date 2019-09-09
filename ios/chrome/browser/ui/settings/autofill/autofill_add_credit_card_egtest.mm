// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/app_launch_manager.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::AddPaymentMethodButton;
using chrome_test_util::PaymentMethodsButton;

// Tests for Settings Autofill add credit cards section.
@interface AutofillAddCreditCardTestCase : ChromeTestCase
@end

@implementation AutofillAddCreditCardTestCase

- (void)setUp {
  [super setUp];
  [[AppLaunchManager sharedManager]
      ensureAppLaunchedWithFeaturesEnabled:{kSettingsAddPaymentMethod}
                                  disabled:{}
                              forceRestart:NO];
  GREYAssertTrue([ChromeEarlGrey isSettingsAddPaymentMethodEnabled],
                 @"SettingsAddPaymentMethod should be enabled");
  [ChromeEarlGrey openNewTab];
  [ChromeEarlGreyUI openSettingsMenu];
  [ChromeEarlGreyUI tapSettingsMenuButton:PaymentMethodsButton()];
  [[EarlGrey selectElementWithMatcher:AddPaymentMethodButton()]
      performAction:grey_tap()];
}

@end
