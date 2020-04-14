// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/proto_conversion.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "chrome/browser/upboarding/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace upboarding {
namespace {

void TestQueryTileEntryConversion(QueryTileEntry& expected) {
  upboarding::query_tiles::proto::QueryTileEntry proto;
  QueryTileEntry actual;
  QueryTileEntryToProto(&expected, &proto);
  QueryTileEntryFromProto(&proto, &actual);
  EXPECT_TRUE(expected == actual)
      << "actual: \n"
      << test::DebugString(&actual) << "expected: \n"
      << test::DebugString(&expected);
}

TEST(ProtoConversionTest, QueryTileEntryConversion) {
  QueryTileEntry entry;
  entry.id = "test-guid-root";
  entry.query_text = "test query str";
  entry.display_text = "test display text";
  entry.accessibility_text = "read this test display text";
  entry.image_metadatas.emplace_back("image-test-id-1",
                                     GURL("www.example.com"));
  entry.image_metadatas.emplace_back("image-test-id-2",
                                     GURL("www.fakeurl.com"));
  auto entry1 = std::make_unique<QueryTileEntry>();
  entry1->id = "test-guid-001";
  auto entry2 = std::make_unique<QueryTileEntry>();
  entry2->id = "test-guid-002";
  auto entry3 = std::make_unique<QueryTileEntry>();
  entry3->id = "test-guid-003";
  entry1->sub_tiles.emplace_back(std::move(entry3));
  entry.sub_tiles.emplace_back(std::move(entry1));
  entry.sub_tiles.emplace_back(std::move(entry2));
  TestQueryTileEntryConversion(entry);
}

}  // namespace

}  // namespace upboarding
