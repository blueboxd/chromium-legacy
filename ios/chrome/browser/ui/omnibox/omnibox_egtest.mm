// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/bind.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_matchers_app_interface.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/chrome/test/earl_grey/hardware_keyboard_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Web page 1.
const char kPage1[] = "This is the first page";
const char kPage1Title[] = "Title 1";
const char kPage1URL[] = "/page1.html";

// Web page 2.
const char kPage2[] = "This is the second page";
const char kPage2Title[] = "Title 2";
const char kPage2URL[] = "/page2.html";

// Provides responses for the different pages.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kPage1URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage1Title) +
        "</title></head><body>" + std::string(kPage1) + "</body></html>");
    return std::move(http_response);
  }

  if (request.relative_url == kPage2URL) {
    http_response->set_content(
        "<html><head><title>" + std::string(kPage2Title) +
        "</title></head><body>" + std::string(kPage2) + "</body></html>");
    return std::move(http_response);
  }

  return nil;
}

}  //  namespace

#pragma mark - Steady state tests

@interface LocationBarSteadyStateTestCase : ChromeTestCase
@end

@implementation LocationBarSteadyStateTestCase

- (void)setUp {
  [super setUp];

  // Start a server to be able to navigate to a web page.
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");

  [ChromeEarlGrey clearBrowsingHistory];

  // Clear the pasteboard in case there is a URL copied.
  UIPasteboard* pasteboard = UIPasteboard.generalPasteboard;
  [pasteboard setValue:@"" forPasteboardType:UIPasteboardNameGeneral];
}

// Tapping on steady view starts editing.
- (void)testTapSwitchesToEditing {
  [self openPage1];

  [ChromeEarlGreyUI focusOmnibox];
  [self checkLocationBarEditState];
}

// Tests that in compact, a share button is visible.
// Voice search is not enabled on the bots, so the voice search button is
// not tested here.
- (void)testTrailingButton {
  [self openPage1];

  if ([ChromeEarlGrey isCompactWidth]) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ShareButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }
}

- (void)testCopyPaste {
  [self openPage1];

  // Long pressing should allow copying.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];

  // Verify that system text selection callout is displayed.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_notNil()];

  [self checkLocationBarSteadyState];

  // Tapping it should copy the URL.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCopyButton()]
      performAction:grey_tap()];

  // Edit menu takes a while to copy, and not waiting here will cause Page 2 to
  // load before the copy happens, so Page 2 URL may be copied.
  GREYCondition* copyCondition = [GREYCondition
      conditionWithName:@"page1 URL copied condition"
                  block:^BOOL {
                    return [UIPasteboard.generalPasteboard.string
                        hasSuffix:base::SysUTF8ToNSString(kPage1URL)];
                  }];
  // Wait for copy to happen or timeout after 5 seconds.
  BOOL success = [copyCondition waitWithTimeout:5];
  GREYAssertTrue(success, @"Copying page 1 URL failed");

  // Go to another web page.
  [self openPage2];

  // Visit copied link should now be available.
  NSString* a11yLabelPasteGo =
      l10n_util::GetNSString(IDS_IOS_VISIT_COPIED_LINK);
  id<GREYMatcher> pasteAndGoMatcher =
      grey_allOf(grey_accessibilityLabel(a11yLabelPasteGo),
                 chrome_test_util::SystemSelectionCallout(), nil);
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:pasteAndGoMatcher]
      assertWithMatcher:grey_notNil()];

  [self checkLocationBarSteadyState];

  // Tapping it should navigate to Page 1.
  [[EarlGrey selectElementWithMatcher:pasteAndGoMatcher]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForPageToFinishLoading];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
}

- (void)testFocusingOmniboxDismissesEditMenu {
  [self openPage1];

  // Long pressing should open edit menu.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_notNil()];

  // Focus omnibox.
  [ChromeEarlGreyUI focusOmnibox];
  [self checkLocationBarEditState];

  // Verify that the edit menu disappeared.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          SystemSelectionCalloutCopyButton()]
      assertWithMatcher:grey_nil()];
}

// Copies and pastes a URL, then performs an undo of the paste, and attempts to
// perform a second undo.
- (void)testCopyPasteUndo {
  [self openPage1];

  [ChromeEarlGreyUI focusOmnibox];
  [self checkLocationBarEditState];

  chrome_test_util::SimulatePhysicalKeyboardEvent(UIKeyModifierCommand, @"C");

  // Edit menu takes a while to copy, and not waiting here will cause Page 2 to
  // load before the copy happens, so Page 2 URL may be copied.
  GREYCondition* copyCondition = [GREYCondition
      conditionWithName:@"page1 URL copied condition"
                  block:^BOOL {
                    return [UIPasteboard.generalPasteboard.string
                        hasSuffix:base::SysUTF8ToNSString(kPage1URL)];
                  }];
  // Wait for copy to happen or timeout after 5 seconds.
  BOOL success = [copyCondition waitWithTimeout:5];
  GREYAssertTrue(success, @"Copying page 1 URL failed");

  // Defocus the omnibox.
  if ([ChromeEarlGrey isIPadIdiom]) {
    id<GREYMatcher> typingShield = grey_accessibilityID(@"Typing Shield");
    [[EarlGrey selectElementWithMatcher:typingShield] performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:grey_buttonTitle(@"Cancel")]
        performAction:grey_tap()];
  }

  [self openPage2];

  [ChromeEarlGreyUI focusOmnibox];

  // Attempt to paste.
  chrome_test_util::SimulatePhysicalKeyboardEvent(UIKeyModifierCommand, @"V");

  // Verify that paste happened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kPage1URL)];

  // Attempt to undo.
  chrome_test_util::SimulatePhysicalKeyboardEvent(UIKeyModifierCommand, @"Z");

  // Verify that undo happened.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kPage2URL)];

  // Attempt to undo again. Nothing should happen. In the past this could lead
  // to a crash.
  chrome_test_util::SimulatePhysicalKeyboardEvent(UIKeyModifierCommand, @"Z");
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(kPage2URL)];
}

#pragma mark - Helpers

// Navigates to Page 1 in a tab and waits for it to load.
- (void)openPage1 {
  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage1URL)];
  [ChromeEarlGrey waitForWebStateContainingText:kPage1];
}

// Navigates to Page 2 in a tab and waits for it to load.
- (void)openPage2 {
  // Go to a web page to have a normal location bar.
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPage2URL)];
  [ChromeEarlGrey waitForWebStateContainingText:kPage2];
}

// Checks that the location bar is currently in steady state.
- (void)checkLocationBarSteadyState {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_notVisible()];
}

// Checks that the location bar is currently in edit state.
- (void)checkLocationBarEditState {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end
