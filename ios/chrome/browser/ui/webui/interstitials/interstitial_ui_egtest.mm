// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <XCTest/XCTest.h>

#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui_constants.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test case for chrome://interstitials WebUI page.
@interface InterstitialWebUITestCase : ChromeTestCase
@end

@implementation InterstitialWebUITestCase

// Tests that chrome://interstitials loads correctly.
- (void)testLoadInterstitialUI {
  [ChromeEarlGrey loadURL:GURL(kChromeUIIntersitialsURL)];

  [ChromeEarlGrey waitForWebStateContainingText:"Choose an interstitial"];
}

// Tests that chrome://interstitials/ssl loads correctly.
- (void)testLoadSSLInterstitialUI {
  GURL SSLInterstitialURL =
      GURL(kChromeUIIntersitialsURL).Resolve(kChromeInterstitialSslPath);
  [ChromeEarlGrey loadURL:SSLInterstitialURL];

  [ChromeEarlGrey
      waitForWebStateContainingText:"Your connection is not private"];
}

// Tests that chrome://interstitials/captiveportal loads correctly.
- (void)testLoadCaptivePortalInterstitialUI {
  GURL captivePortalInterstitialURL =
      GURL(kChromeUIIntersitialsURL)
          .Resolve(kChromeInterstitialCaptivePortalPath);
  [ChromeEarlGrey loadURL:captivePortalInterstitialURL];

  [ChromeEarlGrey waitForWebStateContainingText:"Connect to Wi-Fi"];
}

@end
