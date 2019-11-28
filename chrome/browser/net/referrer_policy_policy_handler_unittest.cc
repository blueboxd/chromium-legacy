// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/referrer_policy_policy_handler.h"

#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "content/public/common/referrer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class ReferrerPolicyPolicyHandlerTest : public testing::Test {
 protected:
  void SetPolicyValue(const std::string& policy,
                      std::unique_ptr<base::Value> value) {
    policies_.Set(policy, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM, std::move(value), nullptr);
  }
  bool CheckPolicySettings() {
    return handler_.CheckPolicySettings(policies_, &errors_);
  }
  void ApplyPolicySettings() {
    handler_.ApplyPolicySettings(policies_, &prefs_);
  }

  void CheckAndApplyPolicySettings() {
    if (CheckPolicySettings())
      ApplyPolicySettings();
  }

  PolicyErrorMap& errors() { return errors_; }
  PrefValueMap& prefs() { return prefs_; }

 private:
  PolicyMap policies_;
  PolicyErrorMap errors_;
  PrefValueMap prefs_;
  ReferrerPolicyPolicyHandler handler_;
};

TEST_F(ReferrerPolicyPolicyHandlerTest, NotSet) {
  CheckAndApplyPolicySettings();

  // Shouldn't error.
  EXPECT_EQ(errors().size(), 0U);
}

// Sanity check tests to ensure the policy errors have the correct name.
TEST_F(ReferrerPolicyPolicyHandlerTest, WrongType) {
  // Do anything that causes a policy error.
  SetPolicyValue(key::kForceLegacyDefaultReferrerPolicy,
                 std::make_unique<base::Value>(1));

  CheckAndApplyPolicySettings();

  // Should have an error
  ASSERT_EQ(errors().size(), 1U);
  EXPECT_EQ(errors().begin()->first, key::kForceLegacyDefaultReferrerPolicy);
}

TEST_F(ReferrerPolicyPolicyHandlerTest, ValueTrue) {
  SetPolicyValue(key::kForceLegacyDefaultReferrerPolicy,
                 std::make_unique<base::Value>(true));

  CheckAndApplyPolicySettings();
  EXPECT_TRUE(content::Referrer::ShouldForceLegacyDefaultReferrerPolicy());
}

TEST_F(ReferrerPolicyPolicyHandlerTest, ValueFalse) {
  SetPolicyValue(key::kForceLegacyDefaultReferrerPolicy,
                 std::make_unique<base::Value>(false));

  CheckAndApplyPolicySettings();
  EXPECT_FALSE(content::Referrer::ShouldForceLegacyDefaultReferrerPolicy());
}
}  // namespace policy
