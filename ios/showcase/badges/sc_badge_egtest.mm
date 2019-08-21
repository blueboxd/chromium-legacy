// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/ui/badges/badge_constants.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::showcase_utils::Open;
using ::showcase_utils::Close;
}

// Tests for Badges.
@interface SCBadgeTestCase : ShowcaseTestCase
@end

@implementation SCBadgeTestCase

- (void)setUp {
  [super setUp];
  Open(@"Badge View");
}

- (void)tearDown {
  Close();
  [super tearDown];
}

// Tests that the passwords badge and incognito badge are displayed.
- (void)testBadgesVisible {
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kBadgeButtonSavePasswordAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
  [[EarlGrey
      selectElementWithMatcher:
          grey_allOf(grey_accessibilityID(
                         kBadgeButtonSavePasswordAccessibilityIdentifier),
                     grey_sufficientlyVisible(), nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
