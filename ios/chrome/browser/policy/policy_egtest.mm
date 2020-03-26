// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/earl_grey/earl_grey_test.h"

#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/chrome_switches.h"
#import "ios/chrome/browser/policy/policy_egtest_app_interface.h"
#include "ios/chrome/browser/pref_names.h"
#include "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#include "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/testing/earl_grey/app_launch_configuration.h"
#include "ios/testing/earl_grey/app_launch_manager.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns the value of a given policy, looked up in the current platform policy
// provider.
std::unique_ptr<base::Value> GetPlatformPolicy(const std::string& key) {
  std::string json_representation =
      base::SysNSStringToUTF8([PolicyEGTestAppInterface
          valueForPlatformPolicy:base::SysUTF8ToNSString(key)]);
  JSONStringValueDeserializer deserializer(json_representation);
  return deserializer.Deserialize(/*error_code=*/nullptr,
                                  /*error_message=*/nullptr);
}

}  // namespace

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
  [PolicyEGTestAppInterface setSuggestPolicyEnabled:NO];
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:prefs::kSearchSuggestEnabled],
                  @"Search suggest preference was unexpectedly true");

  // Force the preference on via policy.
  [PolicyEGTestAppInterface setSuggestPolicyEnabled:YES];
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:prefs::kSearchSuggestEnabled],
                 @"Search suggest preference was unexpectedly false");
}

@end

// Test case that uses the production platform policy provider.
@interface PolicyPlatformProviderTestCase : ChromeTestCase
@end

@implementation PolicyPlatformProviderTestCase

- (AppLaunchConfiguration)appConfigurationForTestCase {
  AppLaunchConfiguration config;
  config.additional_args.push_back(std::string("--") +
                                   switches::kEnableEnterprisePolicy);

  // Commandline flags that start with a single "-" are automatically added to
  // the NSArgumentDomain in NSUserDefaults. Set fake policy data that can be
  // read by the production platform policy provider.
  config.additional_args.push_back("-com.apple.configuration.managed");
  config.additional_args.push_back("{ChromePolicy={"
                                   "DefaultSearchProviderName=Test;"
                                   "NotARegisteredPolicy=Unknown;"
                                   "};}");
  config.relaunch_policy = NoForceRelaunchAndResetState;
  return config;
}

// Tests that policies are properly loaded from NSUserDefaults when using the
// production platform policy provider.

// Tests the value of a policy that was explicitly set.
- (void)testProductionPlatformProviderPolicyExplicitlySet {
  std::unique_ptr<base::Value> searchValue =
      GetPlatformPolicy("DefaultSearchProviderName");
  GREYAssertTrue(searchValue && searchValue->is_string(),
                 @"searchSuggestValue was not of type string");
  GREYAssertEqual(searchValue->GetString(), "Test",
                  @"searchSuggestValue had an unexpected value");
}

// Test the value of a policy that exists in the schema but was not explicitly
// set.
- (void)testProductionPlatformProviderPolicyNotSet {
  std::unique_ptr<base::Value> blocklistValue =
      GetPlatformPolicy("URLBlacklist");
  GREYAssertTrue(blocklistValue && blocklistValue->is_none(),
                 @"blocklistValue was unexpectedly present");
}

// Test the value of a policy that was set in the configuration but is unknown
// to the policy system.
- (void)testProductionPlatformProviderPolicyUnknown {
  std::unique_ptr<base::Value> unknownValue =
      GetPlatformPolicy("NotARegisteredPolicy");
  GREYAssertTrue(unknownValue && unknownValue->is_string(),
                 @"unknownValue was not of type string");
  GREYAssertEqual(unknownValue->GetString(), "Unknown",
                  @"unknownValue had an unexpected value");
}

@end

// TODO(crbug.com/1015113): This macro breaks Xcode indexing unless it is placed
// at the bottom of the file or followed by a semicolon.
GREY_STUB_CLASS_IN_APP_MAIN_QUEUE(PolicyEGTestAppInterface)
