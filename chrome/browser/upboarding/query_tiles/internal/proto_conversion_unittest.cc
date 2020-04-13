// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/proto_conversion.h"

#include <type_traits>

#include "chrome/browser/upboarding/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace upboarding {
namespace {

void TestQueryTileEntryConversion(const QueryTileEntry& expected) {
  upboarding::query_tiles::proto::QueryTileEntry proto;
  QueryTileEntry actual;
  QueryTileEntryToProto(std::remove_const<QueryTileEntry*>::type(&expected),
                        &proto);
  QueryTileEntryFromProto(&proto, &actual);
  EXPECT_EQ(expected, actual) << "actual: \n"
                              << test::DebugString(&actual) << "expected: \n"
                              << test::DebugString(&expected);
}

void TestTileGroupConversion(const TileGroup& expected) {
  upboarding::query_tiles::proto::QueryTileGroup proto;
  TileGroup actual;
  TileGroupToProto(std::remove_const<TileGroup*>::type(&expected), &proto);
  TileGroupFromProto(&proto, &actual);
  EXPECT_EQ(expected, actual) << "actual: \n"
                              << test::DebugString(&actual) << "expected: \n"
                              << test::DebugString(&expected);
}

TEST(QueryTileProtoConversionTest, ConvertTileEntryGroudtrip) {
  QueryTileEntry entry;
  test::ResetTestEntry(&entry);
  TestQueryTileEntryConversion(entry);
}

TEST(QueryTileProtoConversionTest, ConvertTileGroupRoundtrip) {
  TileGroup group;
  test::ResetTestGroup(&group);
  TestTileGroupConversion(group);
}

}  // namespace

}  // namespace upboarding
