// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "ios/chrome/browser/system_flags.h"
#import "ios/chrome/browser/tabs/tab_title_util.h"
#import "ios/chrome/browser/ui/tabs/tab_view.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Matcher for the tab title for a given |web_state|.
id<GREYMatcher> TabTitleMatcher(web::WebState* web_state) {
  return grey_text(tab_util::GetTabTitle(web_state));
}
}  // namespace

// Tests for the tab strip shown on iPad.
@interface TabStripTestCase : ChromeTestCase
@end

@implementation TabStripTestCase

// Test switching tabs using the tab strip.
- (void)testTabStripSwitchTabs {
  // Only iPad has a tab strip.
  if (IsCompactWidth()) {
    return;
  }

  // TODO(crbug.com/238112):  Make this test also handle the 'collapsed' tab
  // case.
  const int kNumberOfTabs = 3;
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://about")];
  [ChromeEarlGreyUI openNewTab];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // Note that the tab ordering wraps.  E.g. if A, B, and C are open,
  // and C is the current tab, the 'next' tab is 'A'.
  for (int i = 0; i < kNumberOfTabs + 1; i++) {
    GREYAssertTrue([ChromeEarlGrey mainTabCount] > 1,
                   [ChromeEarlGrey mainTabCount] ? @"Only one tab open."
                                                 : @"No more tabs.");
    web::WebState* nextWebState = chrome_test_util::GetNextWebState();

    [[EarlGrey selectElementWithMatcher:TabTitleMatcher(nextWebState)]
        performAction:grey_tap()];

    web::WebState* newCurrentWebState = chrome_test_util::GetCurrentWebState();
    GREYAssertTrue(newCurrentWebState == nextWebState,
                   @"The selected tab did not change to the next tab.");
  }
}

@end
