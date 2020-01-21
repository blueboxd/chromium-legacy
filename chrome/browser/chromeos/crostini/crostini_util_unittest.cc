// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include <limits>
#include <numeric>
#include <vector>

#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// end is inclusive
std::vector<int64_t> range(int64_t start, int64_t end, int64_t step) {
  std::vector<int64_t> ticks;
  for (int64_t n = start; n <= end; n += step) {
    ticks.emplace_back(n);
  }
  return ticks;
}
}  // namespace

namespace crostini {

class CrostiniUtilTest : public testing::Test {
 public:
  CrostiniUtilTest() = default;
  CrostiniUtilTest(const CrostiniUtilTest&) = delete;
  CrostiniUtilTest& operator=(const CrostiniUtilTest&) = delete;
};

TEST_F(CrostiniUtilTest, GetTicksForDiskSizeSizeZeroRange) {
  // When min_size == available_space the only possible option is
  // available_space.
  std::vector<int64_t> expected(101);
  std::fill(expected.begin(), expected.end(), 10);
  EXPECT_THAT(crostini::GetTicksForDiskSize(10, 10),
              testing::ContainerEq(expected));
}

TEST_F(CrostiniUtilTest, GetTicksForDiskSizeInvalidInputsNoTicks) {
  const std::vector<int64_t> empty = {};
  // If min_size > available_space there's no solution, so we expect an empty
  // range.
  EXPECT_THAT(crostini::GetTicksForDiskSize(100, 10),
              testing::ContainerEq(empty));

  // If any of the inputs are negative we return an empty range.
  EXPECT_THAT(crostini::GetTicksForDiskSize(100, -100),
              testing::ContainerEq(empty));
  EXPECT_THAT(crostini::GetTicksForDiskSize(-100, 100),
              testing::ContainerEq(empty));
  EXPECT_THAT(crostini::GetTicksForDiskSize(-100, -10),
              testing::ContainerEq(empty));
}

TEST_F(CrostiniUtilTest, GetTicksForDiskSizeHappyPath) {
  auto expected = range(100, 1100, 10);
  EXPECT_THAT(crostini::GetTicksForDiskSize(100, 1100),
              testing::ContainerEq(expected));
}

TEST_F(CrostiniUtilTest, GetTicksForDiskSizeNotDivisible) {
  // Ticks are rounded down to integer values.
  auto expected = std::vector<int64_t>{313, 316, 319, 323, 326, 329, 333};
  EXPECT_THAT(crostini::GetTicksForDiskSize(0, 333),
              testing::IsSupersetOf(expected));
}

}  // namespace crostini
