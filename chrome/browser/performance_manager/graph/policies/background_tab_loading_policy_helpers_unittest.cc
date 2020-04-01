// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/graph/policies/background_tab_loading_policy_helpers.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

namespace policies {

namespace {

class BackgroundTabLoadingPolicyHelpersTest : public ::testing::Test {};

}  // namespace

TEST_F(BackgroundTabLoadingPolicyHelpersTest,
       CalculateMaxSimultaneousTabLoads) {
  // Test the lower bound is enforced.
  EXPECT_EQ(10u, CalculateMaxSimultaneousTabLoads(
                     10 /* lower_bound */, 20 /* upper_bound */,
                     1 /* cores_per_load */, 1 /* cores */));

  // Test the upper bound is enforced.
  EXPECT_EQ(20u, CalculateMaxSimultaneousTabLoads(
                     10 /* lower_bound */, 20 /* upper_bound */,
                     1 /* cores_per_load */, 30 /* cores */));

  // Test the per-core calculation is correct.
  EXPECT_EQ(15u, CalculateMaxSimultaneousTabLoads(
                     10 /* lower_bound */, 20 /* upper_bound */,
                     1 /* cores_per_load */, 15 /* cores */));
  EXPECT_EQ(15u, CalculateMaxSimultaneousTabLoads(
                     10 /* lower_bound */, 20 /* upper_bound */,
                     2 /* cores_per_load */, 30 /* cores */));

  // If no per-core is specified then upper_bound is returned.
  EXPECT_EQ(5u, CalculateMaxSimultaneousTabLoads(
                    1 /* lower_bound */, 5 /* upper_bound */,
                    0 /* cores_per_load */, 10 /* cores */));

  // If no per-core and no upper_bound is applied, then "upper_bound" is
  // returned.
  EXPECT_EQ(
      std::numeric_limits<size_t>::max(),
      CalculateMaxSimultaneousTabLoads(3 /* lower_bound */, 0 /* upper_bound */,
                                       0 /* cores_per_load */, 4 /* cores */));
}

}  // namespace policies

}  // namespace performance_manager
