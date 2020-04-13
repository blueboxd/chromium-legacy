// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/query_tile_entry.h"

#include <utility>

#include "base/test/task_environment.h"
#include "chrome/browser/upboarding/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace upboarding {
namespace {

TEST(QueryTileEntryTest, CompareOperators) {
  QueryTileEntry lhs, rhs;
  test::ResetTestEntry(&lhs);
  test::ResetTestEntry(&rhs);
  EXPECT_EQ(lhs, rhs);
  EXPECT_FALSE(lhs != rhs);
  // Test any data field changed.
  rhs.id = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.query_text = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.display_text = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.accessibility_text = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  // Test image metadatas changed.
  rhs.image_metadatas.front().id = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.image_metadatas.front().url = GURL("http://www.url-changed.com");
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.image_metadatas.pop_back();
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.image_metadatas.emplace_back(ImageMetadata());
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  // Test children changed.
  rhs.sub_tiles.front()->id = "changed";
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.sub_tiles.pop_back();
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  rhs.sub_tiles.emplace_back(std::make_unique<QueryTileEntry>());
  EXPECT_NE(lhs, rhs);
  test::ResetTestEntry(&rhs);

  std::reverse(rhs.sub_tiles.begin(), rhs.sub_tiles.end());
  EXPECT_EQ(lhs, rhs);
  EXPECT_EQ(lhs, rhs);
}

TEST(QueryTileEntryTest, CopyOperator) {
  QueryTileEntry lhs;
  test::ResetTestEntry(&lhs);
  QueryTileEntry rhs(lhs);
  EXPECT_EQ(lhs, rhs);
}

TEST(QueryTileEntryTest, AssignOperator) {
  QueryTileEntry lhs;
  test::ResetTestEntry(&lhs);
  QueryTileEntry rhs = lhs;
  EXPECT_EQ(lhs, rhs);
}

TEST(QueryTileEntryTest, MoveOperator) {
  QueryTileEntry lhs;
  test::ResetTestEntry(&lhs);
  QueryTileEntry rhs = std::move(lhs);
  EXPECT_EQ(lhs, QueryTileEntry());
  QueryTileEntry expected;
  test::ResetTestEntry(&expected);
  EXPECT_EQ(expected, rhs);
}

}  // namespace

}  // namespace upboarding
