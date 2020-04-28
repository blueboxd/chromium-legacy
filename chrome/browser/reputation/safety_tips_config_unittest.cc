// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chrome/browser/reputation/safety_tips_config.h"

#include "chrome/browser/reputation/safety_tip_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(SafetyTipsConfigTest, TestUrlAllowlist) {
  SetSafetyTipAllowlistPatterns({"example.com/"}, {});
  auto* config = GetSafetyTipsRemoteConfigProto();
  EXPECT_TRUE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://example.com")));
  EXPECT_FALSE(IsUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://example.org")));
}

TEST(SafetyTipsConfigTest, TestTargetUrlAllowlist) {
  SetSafetyTipAllowlistPatterns({}, {"exa.*\\.com"});
  auto* config = GetSafetyTipsRemoteConfigProto();
  EXPECT_TRUE(IsTargetUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://example.com")));
  EXPECT_FALSE(IsTargetUrlAllowlistedBySafetyTipsComponent(
      config, GURL("http://example.org")));
}
