// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/query_tile_entry.h"

#include "base/test/task_environment.h"
#include "chrome/browser/upboarding/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace upboarding {
namespace {

void ResetTestCase(QueryTileEntry& entry) {
  entry.id = "test-guid-root";
  entry.query_text = "test query str";
  entry.display_text = "test display text";
  entry.accessibility_text = "read this test display text";
  entry.image_metadatas.clear();
  entry.image_metadatas.emplace_back("image-test-id-1",
                                     GURL("http://www.example.com"));
  entry.image_metadatas.emplace_back("image-test-id-2",
                                     GURL("http://www.fakeurl.com"));

  auto entry1 = std::make_unique<QueryTileEntry>();
  entry1->id = "test-guid-001";
  auto entry2 = std::make_unique<QueryTileEntry>();
  entry2->id = "test-guid-002";
  auto entry3 = std::make_unique<QueryTileEntry>();
  entry3->id = "test-guid-003";
  entry1->sub_tiles.clear();
  entry1->sub_tiles.emplace_back(std::move(entry3));
  entry.sub_tiles.clear();
  entry.sub_tiles.emplace_back(std::move(entry1));
  entry.sub_tiles.emplace_back(std::move(entry2));
}

TEST(QueryTileEntryTest, CompareOperators) {
  QueryTileEntry lhs, rhs;
  ResetTestCase(lhs);
  ResetTestCase(rhs);
  EXPECT_EQ(lhs, rhs);
  EXPECT_FALSE(lhs != rhs);
  // Test any data field changed.
  rhs.id = "changed";
  EXPECT_NE(lhs, rhs);
  ResetTestCase(rhs);

  rhs.query_text = "changed";
  EXPECT_NE(lhs, rhs);
  ResetTestCase(rhs);

  rhs.display_text = "changed";
  EXPECT_NE(lhs, rhs);
  ResetTestCase(rhs);

  rhs.accessibility_text = "changed";
  EXPECT_NE(lhs, rhs);
  ResetTestCase(rhs);

  // Test image metadatas changed.
  rhs.image_metadatas.front().id = "changed";
  EXPECT_NE(lhs, rhs);
  ResetTestCase(rhs);

  rhs.image_metadatas.front().url = GURL("http://www.url-changed.com");
  EXPECT_NE(lhs, rhs);
  ResetTestCase(rhs);

  rhs.image_metadatas.pop_back();
  EXPECT_NE(lhs, rhs);
  ResetTestCase(rhs);

  rhs.image_metadatas.emplace_back(ImageMetadata());
  EXPECT_NE(lhs, rhs);
  ResetTestCase(rhs);

  // Test children changed.
  rhs.sub_tiles.front()->id = "changed";
  EXPECT_NE(lhs, rhs);
  ResetTestCase(rhs);

  rhs.sub_tiles.pop_back();
  EXPECT_NE(lhs, rhs);
  ResetTestCase(rhs);

  rhs.sub_tiles.emplace_back(std::make_unique<QueryTileEntry>());
  EXPECT_NE(lhs, rhs);
}

TEST(QueryTileEntryTest, CopyOperator) {
  QueryTileEntry lhs;
  ResetTestCase(lhs);
  QueryTileEntry rhs(lhs);
  EXPECT_EQ(lhs, rhs);
}

TEST(QueryTileEntryTest, AssignOperator) {
  QueryTileEntry lhs;
  ResetTestCase(lhs);
  QueryTileEntry rhs = lhs;
  EXPECT_EQ(lhs, rhs);
}

TEST(QueryTileEntryTest, MoveOperator) {
  QueryTileEntry lhs;
  ResetTestCase(lhs);
  QueryTileEntry rhs = std::move(lhs);
  EXPECT_EQ(lhs, QueryTileEntry());
  QueryTileEntry expected;
  ResetTestCase(expected);
  EXPECT_EQ(expected, rhs);
}

}  // namespace

}  // namespace upboarding
