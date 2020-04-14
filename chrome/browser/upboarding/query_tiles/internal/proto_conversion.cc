// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/proto_conversion.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"

namespace upboarding {

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

}  // namespace upboarding
