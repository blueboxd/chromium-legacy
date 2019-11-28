// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "content/public/common/referrer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

// Toggling the ForceLegacyDefaultReferrerPolicy enterprise policy should flip
// the corresponding content::Referrer global.
class ForceLegacyDefaultReferrerPolicy : public InProcessBrowserTest {
 public:
  void SetUp() override {
    EXPECT_CALL(policy_provider_, IsInitializationComplete(testing::_))
        .WillRepeatedly(testing::Return(true));
    BrowserPolicyConnector::SetPolicyProviderForTesting(&policy_provider_);
    InProcessBrowserTest::SetUp();
  }

 protected:
  MockConfigurationPolicyProvider policy_provider_;
};

IN_PROC_BROWSER_TEST_F(ForceLegacyDefaultReferrerPolicy, UpdatesDynamically) {
  // When the policy's unset, we shouldn't be forcing the legacy default
  // referrer policy.
  EXPECT_FALSE(content::Referrer::ShouldForceLegacyDefaultReferrerPolicy());

  policy::PolicyMap values;
  values.Set(key::kForceLegacyDefaultReferrerPolicy, POLICY_LEVEL_RECOMMENDED,
             POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(true), nullptr);
  policy_provider_.UpdateChromePolicy(values);
  base::RunLoop().RunUntilIdle();
  // When the policy's true, we should have flipped the global to true.
  EXPECT_TRUE(content::Referrer::ShouldForceLegacyDefaultReferrerPolicy());

  values.Set(key::kForceLegacyDefaultReferrerPolicy, POLICY_LEVEL_RECOMMENDED,
             POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
             std::make_unique<base::Value>(false), nullptr);
  policy_provider_.UpdateChromePolicy(values);
  base::RunLoop().RunUntilIdle();
  // When the policy's false, we should have flipped the global back to false.
  EXPECT_FALSE(content::Referrer::ShouldForceLegacyDefaultReferrerPolicy());
}

}  // namespace policy
