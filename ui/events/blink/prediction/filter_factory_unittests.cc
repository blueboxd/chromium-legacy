// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/blink/prediction/filter_factory.h"

namespace ui {
namespace test {

class FilterFactoryTest : public testing::Test {
 public:
  explicit FilterFactoryTest() {}

  DISALLOW_COPY_AND_ASSIGN(FilterFactoryTest);
};

// Check if the FilterType returned is correct
TEST_F(FilterFactoryTest, TestGetFilterType) {
  EXPECT_EQ(
      input_prediction::FilterType::kEmpty,
      FilterFactory::GetFilterTypeFromName(input_prediction::kFilterNameEmpty));
  // Default type Empty
  EXPECT_EQ(input_prediction::FilterType::kEmpty,
            FilterFactory::GetFilterTypeFromName(""));
}

TEST_F(FilterFactoryTest, TestGetFilter) {
  EXPECT_STREQ(input_prediction::kFilterNameEmpty,
               FilterFactory::CreateFilter(input_prediction::FilterType::kEmpty)
                   ->GetName());
}

}  // namespace test
}  // namespace ui