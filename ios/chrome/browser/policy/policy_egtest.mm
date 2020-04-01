// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/earl_grey/earl_grey_test.h"

#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/policy/policy_app_interface.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#include "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/app_launch_configuration.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test case to verify that enterprise policies are set and respected.
@interface PolicyTestCase : ChromeTestCase
@end

@implementation PolicyTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  // Use commandline args to insert fake policy data into NSUserDefaults. To the
  // app, this policy data will appear under the
  // "com.apple.configuration.managed" key.
  AppLaunchConfiguration config;
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableEnterprisePolicy);
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

// Tests that about:policy is available.
- (void)testAboutPolicy {
  [ChromeEarlGrey loadURL:GURL("chrome://policy")];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_SHOW_UNSET)];
}

// Tests for the SearchSuggestEnabled policy.
- (void)testSearchSuggestEnabled {
  // Loading chrome://policy isn't necessary for the test to succeed, but it
  // provides some visual feedback as the test runs.
  [ChromeEarlGrey loadURL:GURL("chrome://policy")];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_SHOW_UNSET)];

  // Verify that the unmanaged pref's default value is true.
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSearchSuggestEnabled],
                 @"Unexpected default value");

  // Force the preference off via policy.
  [PolicyAppInterface setSuggestPolicyEnabled:NO];
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSearchSuggestEnabled],
                  @"Search suggest preference was unexpectedly true");

  // Force the preference on via policy.
  [PolicyAppInterface setSuggestPolicyEnabled:YES];
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSearchSuggestEnabled],
                 @"Search suggest preference was unexpectedly false");
}

@end
