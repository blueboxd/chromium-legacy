// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/sys_string_conversions.h"
#include "components/policy/policy_constants.h"
#import "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#import "ios/chrome/browser/policy/policy_features.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/app_launch_configuration.h"
#import "ios/testing/earl_grey/earl_grey_test.h"
#include "net/base/net_errors.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Tests the URLBlocklist and URLWhitelist enterprise policies.
@interface PolicyURLBlockingTestCase : ChromeTestCase
@end

@implementation PolicyURLBlockingTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config;
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableEnterprisePolicy);
  config.additional_args.push_back(std::string("--") +
                                   switches::kInstallURLBlocklistHandlers);
  config.additional_args.push_back(
      std::string("--enable-features=URLBlocklistIOS,UseJSForErrorPage"));
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

- (void)setUp {
  [super setUp];
  GREYAssertTrue(self.testServer->Start(), @"Test server failed to start.");
}

// Tests that pages are not blocked when the blocklist exists, but is empty.
- (void)testEmptyBlocklist {
  [PolicyAppInterface
      setPolicyValue:@"[]"
              forKey:base::SysUTF8ToNSString(policy::key::kURLBlacklist)];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

// Tests that a page load is blocked when the URLBlocklist policy is set to
// block all URLs.
- (void)testWildcardBlocklist {
  [PolicyAppInterface
      setPolicyValue:@"[\"*\"]"
              forKey:base::SysUTF8ToNSString(policy::key::kURLBlacklist)];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey
      waitForWebStateContainingText:net::ErrorToShortString(
                                        net::ERR_BLOCKED_BY_ADMINISTRATOR)];
}

// Tests that the NTP is not blocked by the wildcard blocklist.
- (void)testNTPIsNotBlocked {
  [PolicyAppInterface
      setPolicyValue:@"[\"*\"]"
              forKey:base::SysUTF8ToNSString(policy::key::kURLBlacklist)];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::FakeOmnibox()]
      assertWithMatcher:grey_sufficientlyVisible()];
}

// Tests that a page is blocked when the URLBlocklist policy is set to block a
// specific URL.
- (void)testExplicitBlocklist {
  [PolicyAppInterface
      setPolicyValue:@"[\"*/echo\"]"
              forKey:base::SysUTF8ToNSString(policy::key::kURLBlacklist)];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey
      waitForWebStateContainingText:net::ErrorToShortString(
                                        net::ERR_BLOCKED_BY_ADMINISTRATOR)];
}

// Tests that pages are loaded when explicitly listed in the URLAllowlist.
- (void)testAllowlist {
  [PolicyAppInterface
      setPolicyValue:@"[\"*\"]"
              forKey:base::SysUTF8ToNSString(policy::key::kURLBlacklist)];
  [PolicyAppInterface
      setPolicyValue:@"[\"*/echo\"]"
              forKey:base::SysUTF8ToNSString(policy::key::kURLWhitelist)];

  [ChromeEarlGrey loadURL:self.testServer->GetURL("/echo")];

  [ChromeEarlGrey waitForWebStateContainingText:"Echo"];
}

@end
