// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/proto_conversion.h"

#include "components/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace upboarding {
namespace {

void TestTileConversion(Tile& expected) {
  upboarding::query_tiles::proto::Tile proto;
  Tile actual;
  TileToProto(&expected, &proto);
  TileFromProto(&proto, &actual);
  EXPECT_TRUE(test::AreTilesIdentical(expected, actual))
      << "actual: \n"
      << test::DebugString(&actual) << "expected: \n"
      << test::DebugString(&expected);
}

void TestTileGroupConversion(TileGroup& expected) {
  upboarding::query_tiles::proto::TileGroup proto;
  TileGroup actual;
  TileGroupToProto(&expected, &proto);
  TileGroupFromProto(&proto, &actual);
  EXPECT_TRUE(test::AreTileGroupsIdentical(expected, actual))
      << "actual: \n"
      << test::DebugString(&actual) << "expected: \n"
      << test::DebugString(&expected);
}

TEST(TileProtoConversionTest, ConvertTileGroudtrip) {
  Tile entry;
  test::ResetTestEntry(&entry);
  TestTileConversion(entry);
}

TEST(TileProtoConversionTest, ConvertTileGroupRoundtrip) {
  TileGroup group;
  test::ResetTestGroup(&group);
  TestTileGroupConversion(group);
}

}  // namespace

}  // namespace upboarding
