// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/browser/extensions_browser_client.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

enum class CorsPolicyTestMode {
  kEnabled,
  kDisabled,
};

class CorsPolicyTest
    : public PolicyTest,
      public ::testing::WithParamInterface<CorsPolicyTestMode> {
 public:
  CorsPolicyTest() {
    switch (GetParam()) {
      case CorsPolicyTestMode::kEnabled:
        scoped_feature_list_.InitWithFeatures(
            {}, {features::kHideCorsLegacyModeEnabledPolicySupport,
                 features::kHideCorsMitigationListPolicySupport});
        break;
      case CorsPolicyTestMode::kDisabled:
        scoped_feature_list_.InitWithFeatures(
            {features::kHideCorsLegacyModeEnabledPolicySupport,
             features::kHideCorsMitigationListPolicySupport},
            {});
        break;
    }
  }

 protected:
  bool ShouldForceWebRequestExtraHeaders() {
    switch (GetParam()) {
      case CorsPolicyTestMode::kEnabled:
        return network::features::ShouldEnableOutOfBlinkCorsForTesting();
      case CorsPolicyTestMode::kDisabled:
        return false;
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// See CorsExtraSafelistedHeaderNamesTest for more complex end to end tests.
IN_PROC_BROWSER_TEST_P(CorsPolicyTest, CorsMitigationExtraHeadersTest) {
  // The list should be initialized as an empty list, but should not be managed.
  PrefService* prefs = browser()->profile()->GetPrefs();
  EXPECT_TRUE(prefs->GetList(prefs::kCorsMitigationList));
  EXPECT_TRUE(prefs->GetList(prefs::kCorsMitigationList)->empty());
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kCorsMitigationList));

  EXPECT_FALSE(extensions::ExtensionsBrowserClient::Get()
                   ->ShouldForceWebRequestExtraHeaders(browser()->profile()));

  PolicyMap policies;
  policies.Set(key::kCorsMitigationList, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::ListValue>(), nullptr);
  UpdateProviderPolicy(policies);

  // Now the list is managed, and it enforces the webRequest API to use the
  // extraHeaders option.
  EXPECT_TRUE(prefs->GetList(prefs::kCorsMitigationList));
  EXPECT_TRUE(prefs->GetList(prefs::kCorsMitigationList)->empty());
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kCorsMitigationList));

  EXPECT_EQ(ShouldForceWebRequestExtraHeaders(),
            extensions::ExtensionsBrowserClient::Get()
                ->ShouldForceWebRequestExtraHeaders(browser()->profile()));
}

IN_PROC_BROWSER_TEST_P(CorsPolicyTest, CorsLegacyModeEnabledConsistencyTest) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  bool is_out_of_blink_cors_enabled = profile->ShouldEnableOutOfBlinkCors();

  // Check initial states.
  EXPECT_FALSE(prefs->GetBoolean(prefs::kCorsLegacyModeEnabled));
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kCorsLegacyModeEnabled));

  // Check if updated policies are reflected. However, |profile| should keep
  // returning a consistent value that returned at the first access.
  PolicyMap policies;
  policies.Set(key::kCorsLegacyModeEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(true), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_TRUE(prefs->GetBoolean(prefs::kCorsLegacyModeEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kCorsLegacyModeEnabled));
  EXPECT_EQ(is_out_of_blink_cors_enabled,
            profile->ShouldEnableOutOfBlinkCors());

  // Flip the value, and check again.
  policies.Set(key::kCorsLegacyModeEnabled, POLICY_LEVEL_MANDATORY,
               POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
               std::make_unique<base::Value>(false), nullptr);
  UpdateProviderPolicy(policies);

  EXPECT_FALSE(prefs->GetBoolean(prefs::kCorsLegacyModeEnabled));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kCorsLegacyModeEnabled));
  EXPECT_EQ(is_out_of_blink_cors_enabled,
            profile->ShouldEnableOutOfBlinkCors());
}

INSTANTIATE_TEST_SUITE_P(EnabledCorsPolicyTest,
                         CorsPolicyTest,
                         testing::Values(CorsPolicyTestMode::kEnabled));

INSTANTIATE_TEST_SUITE_P(DisabledCorsPolicyTest,
                         CorsPolicyTest,
                         testing::Values(CorsPolicyTestMode::kDisabled));

}  // namespace policy
