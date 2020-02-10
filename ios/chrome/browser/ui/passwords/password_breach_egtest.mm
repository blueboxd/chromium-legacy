// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_app_interface.h"
#import "ios/chrome/browser/ui/passwords/password_breach_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1015113): The EG2 macro is breaking indexing for some reason
// without the trailing semicolon.  For now, disable the extra semi warning
// so Xcode indexing works for the egtest.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-extra-semi"
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(PasswordBreachAppInterface);
#pragma clang diagnostic pop

namespace {
id<GREYMatcher> PasswordBreachMatcher() {
  return grey_accessibilityID(kPasswordBreachViewAccessibilityIdentifier);
}
}  // namespace

@interface PasswordBreachTestCase : ChromeTestCase
@end

@implementation PasswordBreachTestCase

#pragma mark - Tests

- (void)testPasswordBreachIsPresented {
  [PasswordBreachAppInterface showPasswordBreach];
  [[EarlGrey selectElementWithMatcher:PasswordBreachMatcher()]
      assertWithMatcher:grey_notNil()];
}

@end
