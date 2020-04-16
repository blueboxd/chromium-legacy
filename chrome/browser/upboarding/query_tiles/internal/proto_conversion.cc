// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/proto_conversion.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"

namespace upboarding {
namespace {
// Helper method to convert base::Time to integer for serialization. Loses
// precision beyond milliseconds.
int64_t TimeToMilliseconds(const base::Time& time) {
  return time.ToDeltaSinceWindowsEpoch().InMilliseconds();
}

// Helper method to convert serialized time as integer to base::Time for
// deserialization. Loses precision beyond milliseconds.
base::Time MillisecondsToTime(int64_t serialized_time_ms) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMilliseconds(serialized_time_ms));
}

}  // namespace

void QueryTileEntryToProto(
    upboarding::QueryTileEntry* entry,
    upboarding::query_tiles::proto::QueryTileEntry* proto) {
  DCHECK(entry);
  DCHECK(proto);
  proto->set_id(entry->id);
  proto->set_query_text(entry->query_text);
  proto->set_display_text(entry->display_text);
  proto->set_accessibility_text(entry->accessibility_text);

  // Set ImageMetadatas.
  for (auto& image : entry->image_metadatas) {
    auto* data = proto->add_image_metadatas();
    data->set_id(image.id);
    data->set_url(image.url.spec());
  }

  // Set children.
  for (auto& subtile : entry->sub_tiles) {
    QueryTileEntryToProto(subtile.get(), proto->add_sub_tiles());
  }
}

void QueryTileEntryFromProto(
    upboarding::query_tiles::proto::QueryTileEntry* proto,
    upboarding::QueryTileEntry* entry) {
  DCHECK(entry);
  DCHECK(proto);
  entry->id = proto->id();
  entry->query_text = proto->query_text();
  entry->display_text = proto->display_text();
  entry->accessibility_text = proto->accessibility_text();

  for (const auto& image_md : proto->image_metadatas()) {
    entry->image_metadatas.emplace_back(image_md.id(), GURL(image_md.url()));
  }

  for (int i = 0; i < proto->sub_tiles_size(); i++) {
    auto sub_tile_proto = proto->sub_tiles(i);
    auto child = std::make_unique<QueryTileEntry>();
    QueryTileEntryFromProto(&sub_tile_proto, child.get());
    entry->sub_tiles.emplace_back(std::move(child));
  }
}

void TileGroupToProto(TileGroup* group,
                      upboarding::query_tiles::proto::QueryTileGroup* proto) {
  proto->set_id(group->id);
  proto->set_locale(group->locale);
  proto->set_last_updated_time_ms(TimeToMilliseconds(group->last_updated_ts));
  for (auto& tile : group->tiles) {
    QueryTileEntryToProto(tile.get(), proto->add_tiles());
  }
}

void TileGroupFromProto(upboarding::query_tiles::proto::QueryTileGroup* proto,
                        TileGroup* group) {
  group->id = proto->id();
  group->locale = proto->locale();
  group->last_updated_ts = MillisecondsToTime(proto->last_updated_time_ms());
  for (int i = 0; i < proto->tiles().size(); i++) {
    auto entry_proto = proto->tiles(i);
    auto child = std::make_unique<QueryTileEntry>();
    QueryTileEntryFromProto(&entry_proto, child.get());
    group->tiles.emplace_back(std::move(child));
  }
}

}  // namespace upboarding
