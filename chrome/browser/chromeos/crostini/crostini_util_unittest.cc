// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_util.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace crostini {

class CrostiniUtilTest : public testing::Test {
 public:
  CrostiniUtilTest() = default;
  CrostiniUtilTest(const CrostiniUtilTest&) = delete;
  CrostiniUtilTest& operator=(const CrostiniUtilTest&) = delete;
};

TEST_F(CrostiniUtilTest, ContainerIdEquality) {
  auto container1 = ContainerId{"test1", "test2"};
  auto container2 = ContainerId{"test1", "test2"};
  auto container3 = ContainerId{"test2", "test1"};

  ASSERT_TRUE(container1 == container2);
  ASSERT_FALSE(container1 == container3);
  ASSERT_FALSE(container2 == container3);
}

}  // namespace crostini
