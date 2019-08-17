// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/realtime/policy_engine.h"

#include "base/test/scoped_feature_list.h"
#include "components/safe_browsing/features.h"
#include "testing/platform_test.h"

namespace safe_browsing {

class RealTimePolicyEngineTest : public PlatformTest {
 public:
  void ForceSetRealTimeLookupPolicy(bool is_enabled) {
    RealTimePolicyEngine::SetEnabled(is_enabled);
  }

  bool IsUserOptedIn() { return RealTimePolicyEngine::IsUserOptedIn(); }
};

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledFetchAllowlist) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kRealTimeUrlLookupFetchAllowlist);
  EXPECT_FALSE(RealTimePolicyEngine::CanPerformFullURLLookup());
}

TEST_F(RealTimePolicyEngineTest, TestCanPerformFullURLLookup_EnabledByPolicy) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kRealTimeUrlLookupFetchAllowlist);
  ForceSetRealTimeLookupPolicy(/* is_enabled= */ true);
  ASSERT_TRUE(RealTimePolicyEngine::is_enabled_by_pref());
  EXPECT_TRUE(RealTimePolicyEngine::CanPerformFullURLLookup());
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledUrlLookup) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kRealTimeUrlLookupEnabled);
  EXPECT_FALSE(RealTimePolicyEngine::CanPerformFullURLLookup());
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_DisabledUserOptin) {
  // This is hardcoded as false for now so ensure that.
  ASSERT_FALSE(IsUserOptedIn());
}

TEST_F(RealTimePolicyEngineTest,
       TestCanPerformFullURLLookup_EnabledMainFrameOnly) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kRealTimeUrlLookupFetchAllowlist);
  ForceSetRealTimeLookupPolicy(/* is_enabled= */ true);
  ASSERT_TRUE(RealTimePolicyEngine::is_enabled_by_pref());

  for (int i = 0; i <= static_cast<int>(content::ResourceType::kMaxValue);
       i++) {
    content::ResourceType resource_type = static_cast<content::ResourceType>(i);
    bool enabled = RealTimePolicyEngine::CanPerformFullURLLookupForResourceType(
        resource_type);
    switch (resource_type) {
      case content::ResourceType::kMainFrame:
        EXPECT_TRUE(enabled);
        break;
      default:
        EXPECT_FALSE(enabled);
        break;
    }
  }
}

}  // namespace safe_browsing
