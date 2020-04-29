// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_group.h"

#include <iostream>
#include <type_traits>
#include <utility>

#include "base/test/task_environment.h"
#include "components/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace upboarding {
namespace {

TEST(TileGroupTest, CompareOperators) {
  TileGroup lhs, rhs;
  test::ResetTestGroup(&lhs);
  test::ResetTestGroup(&rhs);
  EXPECT_EQ(lhs, rhs);

  rhs.id = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestGroup(&rhs);

  rhs.locale = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestGroup(&rhs);

  rhs.last_updated_ts += base::TimeDelta::FromDays(1);
  EXPECT_NE(lhs, rhs);
  test::ResetTestGroup(&rhs);

  rhs.tiles.clear();
  EXPECT_NE(lhs, rhs);
}

TEST(TileGroupTest, DeepCompareOperators) {
  TileGroup lhs, rhs;
  test::ResetTestGroup(&lhs);
  test::ResetTestGroup(&rhs);
  EXPECT_TRUE(test::AreTileGroupsIdentical(lhs, rhs));

  // Verify the order of tiles does not matter.
  std::reverse(rhs.tiles.begin(), rhs.tiles.end());
  EXPECT_TRUE(test::AreTileGroupsIdentical(lhs, rhs));
  test::ResetTestGroup(&rhs);

  // Verify change on children tiles will make them different.
  rhs.tiles.front()->id = "changed";
  EXPECT_FALSE(test::AreTileGroupsIdentical(lhs, rhs));
}

TEST(TileGroupTest, CopyOperator) {
  TileGroup lhs;
  test::ResetTestGroup(&lhs);
  TileGroup rhs = lhs;
  EXPECT_TRUE(test::AreTileGroupsIdentical(lhs, rhs));
}

TEST(TileGroupTest, MoveOperator) {
  TileGroup lhs;
  test::ResetTestGroup(&lhs);
  TileGroup rhs = std::move(lhs);
  TileGroup expected;
  test::ResetTestGroup(&expected);
  EXPECT_TRUE(test::AreTileGroupsIdentical(expected, rhs));
}

}  // namespace

}  // namespace upboarding
