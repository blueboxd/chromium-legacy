// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ios/ios_util.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_view_controller.h"
#import "ios/showcase/test/showcase_eg_utils.h"
#import "ios/showcase/test/showcase_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

id<GREYMatcher> TitleMatcher() {
  return grey_accessibilityID(kConfirmationAlertTitleAccessibilityIdentifier);
}

id<GREYMatcher> SubtitleMatcher() {
  return grey_accessibilityID(
      kConfirmationAlertSubtitleAccessibilityIdentifier);
}

id<GREYMatcher> PrimaryActionButtonMatcher() {
  return grey_accessibilityID(
      kConfirmationAlertPrimaryActionAccessibilityIdentifier);
}

id<GREYMatcher> MoreInfoButtonMatcher() {
  return grey_accessibilityID(
      kConfirmationAlertMoreInfoAccessibilityIdentifier);
}

}  // namespace

// Tests for the suggestions view controller.
@interface SCCredentialProviderTestCase : ShowcaseTestCase
@end

@implementation SCCredentialProviderTestCase

// Tests ConsentViewController.
- (void)testConsentScreen {
  showcase_utils::Open(@"ConsentViewController");
  [[EarlGrey selectElementWithMatcher:TitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:SubtitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:PrimaryActionButtonMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:MoreInfoButtonMatcher()]
      assertWithMatcher:grey_interactable()];

  showcase_utils::Close();
}

// Tests ConsentViewController.
- (void)testEmptyCredentialsScreen {
  showcase_utils::Open(@"EmptyCredentialsViewController");
  [[EarlGrey selectElementWithMatcher:TitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:SubtitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:PrimaryActionButtonMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:MoreInfoButtonMatcher()]
      assertWithMatcher:grey_nil()];

  showcase_utils::Close();
}

// Tests ConsentViewController.
- (void)testStaleCredentialsScreen {
  showcase_utils::Open(@"StaleCredentialsViewController");
  [[EarlGrey selectElementWithMatcher:TitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:SubtitleMatcher()]
      assertWithMatcher:grey_interactable()];
  [[EarlGrey selectElementWithMatcher:PrimaryActionButtonMatcher()]
      assertWithMatcher:grey_nil()];
  [[EarlGrey selectElementWithMatcher:MoreInfoButtonMatcher()]
      assertWithMatcher:grey_nil()];

  showcase_utils::Close();
}

@end
