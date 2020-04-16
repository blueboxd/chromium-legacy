// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "printing/print_settings.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace printing {

TEST(PrintSettingsTest, IsColorModelSelected) {
  for (int model = UNKNOWN_COLOR_MODEL + 1; model <= COLOR_MODEL_LAST; ++model)
    EXPECT_TRUE(IsColorModelSelected(IsColorModelSelected(model).has_value()));
}

TEST(PrintSettingsDeathTest, IsColorModelSelectedUnknown) {
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  EXPECT_DCHECK_DEATH(IsColorModelSelected(UNKNOWN_COLOR_MODEL));
  EXPECT_DCHECK_DEATH(IsColorModelSelected(UNKNOWN_COLOR_MODEL - 1));
  EXPECT_DCHECK_DEATH(IsColorModelSelected(COLOR_MODEL_LAST + 1));
}

}  // namespace printing
