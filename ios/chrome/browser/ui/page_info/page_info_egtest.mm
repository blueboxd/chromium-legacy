// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "ios/chrome/browser/ui/page_info/page_info_constants.h"
#import "ios/chrome/browser/ui/popup_menu/popup_menu_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PageInfoTestCase : ChromeTestCase
@end

@implementation PageInfoTestCase

// Tests that rotating the device will automatically dismiss the page info view.
- (void)testShowPageInfoAndDismissOnDeviceRotation {
  // TODO(crbug.com/652465): Enable the test for iPad when rotation bug is
  // fixed.
  if ([ChromeEarlGrey isIPadIdiom]) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to device rotation bug.");
  }

  if ([[UIDevice currentDevice] orientation] != UIDeviceOrientationPortrait) {
#if defined(CHROME_EARL_GREY_1)
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait
                             errorOrNil:nil];
#elif defined(CHROME_EARL_GREY_2)
    [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationPortrait error:nil];
#else
#error Neither CHROME_EARL_GREY_1 nor CHROME_EARL_GREY_2 are defined
#endif
  }

  [ChromeEarlGrey loadURL:GURL("https://invalid")];
  [ChromeEarlGreyUI openToolsMenu];
  [[[EarlGrey
      selectElementWithMatcher:grey_allOf(grey_accessibilityID(
                                              kToolsMenuSiteInformation),
                                          grey_sufficientlyVisible(), nil)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 200)
      onElementWithMatcher:grey_accessibilityID(kPopupMenuToolsMenuTableViewId)]
      performAction:grey_tap()];

  // Expect that the page info view has appeared.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()];

#if defined(CHROME_EARL_GREY_1)
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                           errorOrNil:nil];
#elif defined(CHROME_EARL_GREY_2)
  [EarlGrey rotateDeviceToOrientation:UIDeviceOrientationLandscapeRight
                                error:nil];
#else
#error Neither CHROME_EARL_GREY_1 nor CHROME_EARL_GREY_2 are defined
#endif

  // Expect that the page info view has disappeared.
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(
                                          kPageInfoViewAccessibilityIdentifier)]
      assertWithMatcher:grey_nil()];
}

@end
