// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Path which leads to a PDF file.
const char kPDFPath[] = "/testpage.pdf";

}  // namespace

// Tests Open in Feature for PDF files.
@interface OpenInManagerTestCase : ChromeTestCase
@end

@implementation OpenInManagerTestCase

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that open in button appears when opening a PDF, and that tapping on it
// will open the activity view.
- (void)testOpenIn {
  [ChromeEarlGrey loadURL:self.testServer->GetURL(kPDFPath)];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      performAction:grey_tap()];
  // Check and tap on the Cancel button that appears below the activity menu.
  // Other actions buttons can't be checked from EarlGrey.
  [[[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      assertWithMatcher:grey_interactable()] performAction:grey_tap()];

  // Check that after dismissing the activity view, tapping on the web view will
  // bring the open in bar again.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::WebViewMatcher()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OpenInButton()]
      assertWithMatcher:grey_interactable()];
}

@end
