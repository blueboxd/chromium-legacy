// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/proto_conversion.h"

#include <string>
#include <utility>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace upboarding {
namespace {

void TestQueryTileEntryConversion(QueryTileEntry* expected) {
  DCHECK(expected);
  upboarding::query_tiles::proto::QueryTileEntry proto;
  QueryTileEntry actual;
  QueryTileEntryToProto(expected, &proto);
  QueryTileEntryFromProto(&proto, &actual);
  EXPECT_EQ(*expected, actual);
}

TEST(ProtoConversionTest, QueryTileEntryConversion) {
  QueryTileEntry entry;
  entry.id = "test-guid-001";
  entry.query_text = "test query str";
  entry.display_text = "test display text";
  entry.accessibility_text = "read this test display text";
  entry.image_metadatas.emplace_back("image-test-id-1",
                                     GURL("www.example.com"));
  entry.image_metadatas.emplace_back("image-test-id-2",
                                     GURL("www.fakeurl.com"));
  entry.children.emplace("test-guid-002");
  entry.children.emplace("test-guid-003");
  entry.children.emplace("test-guid-004");
  TestQueryTileEntryConversion(&entry);
}

}  // namespace

}  // namespace upboarding
