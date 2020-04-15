// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PrintSettingsTest, IsColorModelSelected) {
  {
    base::Optional<bool> color(IsColorModelSelected(COLOR));
    ASSERT_TRUE(color.has_value());
    EXPECT_TRUE(color.value());
  }
  {
    base::Optional<bool> gray(IsColorModelSelected(GRAY));
    ASSERT_TRUE(gray.has_value());
    EXPECT_FALSE(gray.value());
  }
  {
    // Test lower bound validity.
    base::Optional<bool> lower(IsColorModelSelected(UNKNOWN_COLOR_MODEL + 1));
    EXPECT_TRUE(lower.has_value());
  }
  {
    // Test upper bound validity.
    base::Optional<bool> upper(IsColorModelSelected(COLOR_MODEL_LAST));
    EXPECT_TRUE(upper.has_value());
  }
}

TEST(PrintSettingsDeathTest, IsColorModelSelectedUnknown) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DCHECK_DEATH(IsColorModelSelected(UNKNOWN_COLOR_MODEL));
  EXPECT_DCHECK_DEATH(IsColorModelSelected(UNKNOWN_COLOR_MODEL - 1));
  EXPECT_DCHECK_DEATH(IsColorModelSelected(COLOR_MODEL_LAST + 1));
}

}  // namespace printing
