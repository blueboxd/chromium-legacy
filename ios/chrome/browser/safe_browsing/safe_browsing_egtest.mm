// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/sys_string_conversions.h"
#include "components/safe_browsing/core/features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#import "ios/web/common/features.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests Safe Browsing URL blocking.
@interface SafeBrowsingTestCase : ChromeTestCase {
  // A URL that is treated as an unsafe phishing page.
  GURL _phishingURL;
  // Text that is found on the phishing page.
  std::string _phishingContent;
  // A URL that is treated as an unsafe malware page.
  GURL _malwareURL;
  // Text that is found on the malware page.
  std::string _malwareContent;
  // A URL that is treated as a safe page.
  GURL _safeURL;
  // Text that is found on the safe page.
  std::string _safeContent;
}
@end

@implementation SafeBrowsingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.features_enabled.push_back(safe_browsing::kSafeBrowsingAvailableOnIOS);
  config.features_enabled.push_back(web::features::kSSLCommittedInterstitials);

  // Use commandline args to insert fake unsafe URLs into the Safe Browsing
  // database.
  config.additional_args.push_back(std::string("--mark_as_phishing=") +
                                   _phishingURL.spec());
  config.additional_args.push_back(std::string("--mark_as_malware=") +
                                   _malwareURL.spec());
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)setUp {
  bool started = self.testServer->Start();
  _phishingURL = self.testServer->GetURL("/set-invalid-cookie");
  _phishingContent = "TEST";

  _malwareURL = self.testServer->GetURL("/defaultresponse");
  _malwareContent = "Default";

  _safeURL = self.testServer->GetURL("/echo");
  _safeContent = "Echo";

  // |appConfigurationForTestCase| is called during [super setUp], and
  // depends on the URLs initialized above.
  [super setUp];

  // GREYAssertTrue cannot be called before [super setUp].
  GREYAssertTrue(started, @"Test server failed to start.");
}

// Tests that safe pages are not blocked.
- (void)testSafePage {
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
}

// Tests that a phishing page is blocked, and the "Back to safety" button on
// the warning page works as expected.
- (void)testPhishingPage {
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Deceptive site ahead"];

  // Tap the "Back to safety" button.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
}

// Tests expanding the details on a phishing warning, and proceeding past the
// warning.
- (void)testProceedingPastPhishingWarning {
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Deceptive site ahead"];

  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:
                      "Google Safe Browsing recently detected phishing"];

  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_phishingContent];
}

// Tests that a malware page is blocked, and the "Back to safety" button on the
// warning page works as expected.
- (void)testMalwarePage {
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"The site ahead contains malware"];

  // Tap the "Back to safety" button.
  [ChromeEarlGrey tapWebStateElementWithID:@"primary-button"];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];
}

// Tests expanding the details on a malware warning, and proceeding past the
// warning.
- (void)testProceedingPastMalwareWarning {
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  [ChromeEarlGrey loadURL:_malwareURL];
  [ChromeEarlGrey
      waitForWebStateContainingText:"The site ahead contains malware"];

  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:
                      "Google Safe Browsing recently detected malware"];

  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_malwareContent];
}

// Tests displaying a warning for an unsafe page in incognito mode, and
// proceeding past the warning.
- (void)testWarningInIncognito {
  [ChromeEarlGrey openNewIncognitoTab];
  [ChromeEarlGrey loadURL:_safeURL];
  [ChromeEarlGrey waitForWebStateContainingText:_safeContent];

  [ChromeEarlGrey loadURL:_phishingURL];
  [ChromeEarlGrey waitForWebStateContainingText:"Deceptive site ahead"];

  [ChromeEarlGrey tapWebStateElementWithID:@"details-button"];
  [ChromeEarlGrey waitForWebStateContainingText:
                      "Google Safe Browsing recently detected phishing"];

  [ChromeEarlGrey tapWebStateElementWithID:@"proceed-link"];
  [ChromeEarlGrey waitForWebStateContainingText:_phishingContent];
}

@end
