// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/testing/earl_grey/earl_grey_test.h"

#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/strings/sys_string_conversions.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/policy/policy_constants.h"
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

namespace {

// Returns a JSON-encoded string representing the given |base::Value|. If
// |value| is nullptr, returns a string representing a |base::Value| of type
// NONE.
NSString* SerializeValue(const base::Value value) {
  std::string serialized_value;
  JSONStringValueSerializer serializer(&serialized_value);
  serializer.Serialize(std::move(value));
  return base::SysUTF8ToNSString(serialized_value);
}

// Sets the value of the policy with the |policy_key| key to the given value.
// The value must be serialized as a JSON string.
// Prefer using the other type-specific helpers instead of this generic helper
// if possible.
void SetPolicy(NSString* json_value, const std::string& policy_key) {
  [PolicyAppInterface setPolicyValue:json_value
                              forKey:base::SysUTF8ToNSString(policy_key)];
}

// Sets the value of the policy with the |policy_key| key to the given value.
// The value must be wrapped in a |base::Value|.
// Prefer using the other type-specific helpers instead of this generic helper
// if possible.
void SetPolicy(base::Value value, const std::string& policy_key) {
  SetPolicy(SerializeValue(std::move(value)), policy_key);
}

// Sets the value of the policy with the |policy_key| key to the given boolean
// value.
void SetPolicy(bool enabled, const std::string& policy_key) {
  SetPolicy(base::Value(enabled), policy_key);
}

// TODO(crbug.com/1065522): Add helpers as needed for:
//    - INTEGER
//    - STRING
//    - LIST (and subtypes, e.g. int list, string list, etc)
//    - DICTIONARY (and subtypes, e.g. int list, string list, etc)
//    - Deleting a policy value
//    - Setting multiple policies at once

// Verifies that a bool type policy sets the pref properly.
void VerifyBoolPolicy(const std::string& policy_key,
                      const std::string& pref_name) {
  // Loading chrome://policy isn't necessary for the test to succeed, but it
  // provides some visual feedback as the test runs.
  [ChromeEarlGrey loadURL:GURL("chrome://policy")];
  [ChromeEarlGrey waitForWebStateContainingText:l10n_util::GetStringUTF8(
                                                    IDS_POLICY_SHOW_UNSET)];
  // Force the preference off via policy.
  SetPolicy(false, policy_key);
  GREYAssertFalse([ChromeEarlGrey userBooleanPref:pref_name],
                  @"Preference was unexpectedly true");

  // Force the preference on via policy.
  SetPolicy(true, policy_key);
  GREYAssertTrue([ChromeEarlGrey userBooleanPref:pref_name],
                 @"Preference was unexpectedly false");
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
  VerifyBoolPolicy(policy::key::kSearchSuggestEnabled,
                   prefs::kSearchSuggestEnabled);
}

// Tests for the PasswordManagerEnabled policy.
- (void)testPasswordManagerEnabled {
  VerifyBoolPolicy(policy::key::kPasswordManagerEnabled,
                   password_manager::prefs::kCredentialsEnableService);
}

@end
