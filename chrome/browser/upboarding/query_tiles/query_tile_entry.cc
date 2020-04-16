// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/query_tile_entry.h"

#include <utility>

namespace upboarding {
namespace {

void DeepCopyTiles(const QueryTileEntry& input, QueryTileEntry* out) {
  DCHECK(out);

  out->id = input.id;
  out->display_text = input.display_text;
  out->query_text = input.query_text;
  out->accessibility_text = input.accessibility_text;
  out->image_metadatas = input.image_metadatas;
  out->sub_tiles.clear();
  for (const auto& child : input.sub_tiles) {
    auto entry = std::make_unique<QueryTileEntry>();
    DeepCopyTiles(*child.get(), entry.get());
    out->sub_tiles.emplace_back(std::move(entry));
  }
}

}  // namespace

ImageMetadata::ImageMetadata() = default;

ImageMetadata::ImageMetadata(const std::string& id, const GURL& url)
    : id(id), url(url) {}

ImageMetadata::~ImageMetadata() = default;

ImageMetadata::ImageMetadata(const ImageMetadata& other) = default;

bool ImageMetadata::operator==(const ImageMetadata& other) const {
  return id == other.id && url == other.url;
}

bool QueryTileEntry::operator==(const QueryTileEntry& other) const {
  return id == other.id && display_text == other.display_text &&
         query_text == other.query_text &&
         accessibility_text == other.accessibility_text &&
         image_metadatas.size() == other.image_metadatas.size() &&
         sub_tiles.size() == other.sub_tiles.size();
}

bool QueryTileEntry::operator!=(const QueryTileEntry& other) const {
  return !(*this == other);
}

QueryTileEntry::QueryTileEntry(const QueryTileEntry& other) {
  DeepCopyTiles(other, this);
}

QueryTileEntry::QueryTileEntry() = default;

QueryTileEntry::QueryTileEntry(QueryTileEntry&& other) = default;

QueryTileEntry::~QueryTileEntry() = default;

QueryTileEntry& QueryTileEntry::operator=(const QueryTileEntry& other) {
  DeepCopyTiles(other, this);
  return *this;
}

QueryTileEntry& QueryTileEntry::operator=(QueryTileEntry&& other) = default;

}  // namespace upboarding
