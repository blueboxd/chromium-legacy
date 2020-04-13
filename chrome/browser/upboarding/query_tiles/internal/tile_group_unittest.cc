// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/tile_group.h"

#include <utility>

#include "base/test/task_environment.h"
#include "chrome/browser/upboarding/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace upboarding {
namespace {

TEST(QueryTileGroupTest, CompareOperators) {
  TileGroup lhs, rhs;
  test::ResetTestGroup(&lhs);
  test::ResetTestGroup(&rhs);
  EXPECT_EQ(lhs, rhs);

  rhs.locale = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestGroup(&rhs);

  rhs.locale = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestGroup(&rhs);

  rhs.last_updated_ts += base::TimeDelta::FromMinutes(1);
  EXPECT_NE(lhs, rhs);
  test::ResetTestGroup(&rhs);

  std::reverse(rhs.tiles.begin(), rhs.tiles.end());
  EXPECT_EQ(lhs, rhs);
  test::ResetTestGroup(&rhs);

  rhs.tiles.clear();
  EXPECT_NE(lhs, rhs);
  test::ResetTestGroup(&rhs);

  rhs.tiles.front()->id = "changed";
  EXPECT_NE(lhs, rhs);
}

TEST(QueryTileGroupTest, CopyOperator) {
  TileGroup lhs;
  test::ResetTestGroup(&lhs);
  TileGroup rhs = lhs;
  EXPECT_EQ(lhs, rhs);
}

TEST(QueryTileGroupTest, MoveOperator) {
  TileGroup lhs;
  test::ResetTestGroup(&lhs);
  TileGroup rhs = std::move(lhs);
  EXPECT_EQ(lhs, TileGroup());
  TileGroup expected;
  test::ResetTestGroup(&expected);
  EXPECT_EQ(expected, rhs);
}

}  // namespace

}  // namespace upboarding
