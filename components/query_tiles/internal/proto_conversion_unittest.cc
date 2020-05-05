// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/proto_conversion.h"

#include "base/strings/string_number_conversions.h"
#include "components/query_tiles/internal/tile_iterator.h"
#include "components/query_tiles/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace upboarding {
namespace {

constexpr char kTestLocale[] = "en";

std::string BuildIdForSampleTile(size_t level, size_t pos) {
  return base::NumberToString(level) + "-" + base::NumberToString(pos);
}

void VerifySampleTileId(int level, int pos, const std::string& id) {
  auto expect_id = BuildIdForSampleTile(level, pos);
  EXPECT_EQ(expect_id, id);
}

// Build a fake two level response proto.
void InitResponseProto(ResponseGroupProto* response,
                       size_t num_tiles_each_tile) {
  response->set_locale(kTestLocale);
  // Add top level tiles.
  for (size_t i = 0; i < num_tiles_each_tile; i++) {
    auto* new_top_level_tile = response->add_tiles();
    new_top_level_tile->set_tile_id(BuildIdForSampleTile(0, i));
    new_top_level_tile->set_is_top_level(true);
    // Add sub-tiles.
    for (size_t j = 0; j < num_tiles_each_tile; j++) {
      auto new_tile_id = BuildIdForSampleTile(1, i * num_tiles_each_tile + j);
      new_top_level_tile->add_sub_tile_ids(new_tile_id);
      auto* new_tile = response->add_tiles();
      new_tile->set_tile_id(new_tile_id);
      new_tile->set_is_top_level(false);
    }
  }
}

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

TEST(TileProtoConversionTest, TileConversions) {
  Tile entry;
  test::ResetTestEntry(&entry);
  TestTileConversion(entry);
}

TEST(TileProtoConversionTest, TileGroupConversions) {
  TileGroup group;
  test::ResetTestGroup(&group);
  TestTileGroupConversion(group);
}

TEST(TileProtoConversionTest, TileGroupFromResponseConversions) {
  ResponseGroupProto server_response;
  const int num_tiles_each_tile = 3;
  InitResponseProto(&server_response, num_tiles_each_tile);
  TileGroup tile_group;
  TileGroupFromResponse(server_response, &tile_group);
  std::string server_response_str;
  server_response.SerializeToString(&server_response_str);
  EXPECT_EQ(tile_group.locale, "en") << std::endl
                                     << server_response_str << std::endl
                                     << test::DebugString(&tile_group);
  TileIterator iter(tile_group, TileIterator::kAllTiles);
  size_t count = 0;
  while (iter.HasNext()) {
    const auto* next = iter.Next();
    int level = count >= num_tiles_each_tile ? 1 : 0;
    int pos = count - (level == 0 ? 0 : num_tiles_each_tile);
    VerifySampleTileId(level, pos, next->id);
    count++;
  }
  EXPECT_EQ(count, 12u) << test::DebugString(&tile_group);
}

}  // namespace

}  // namespace upboarding
