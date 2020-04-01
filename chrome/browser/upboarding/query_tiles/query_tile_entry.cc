// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/query_tile_entry.h"

#include <utility>

namespace upboarding {
namespace {

void DeepCopyTiles(const QueryTileEntry* input, QueryTileEntry* out) {
  if (!input || !out)
    return;

  out->id = input->id;
  out->display_text = input->display_text;
  out->query_text = input->query_text;
  out->accessibility_text = input->accessibility_text;
  out->image_metadatas = input->image_metadatas;
  out->sub_tiles.clear();
  for (const auto& child : input->sub_tiles) {
    auto entry = std::make_unique<QueryTileEntry>();
    DeepCopyTiles(child.get(), entry.get());
    out->sub_tiles.emplace_back(std::move(entry));
  }
}

bool AreTreesIdentical(const QueryTileEntry* lhs, const QueryTileEntry* rhs) {
  if (!lhs && !rhs)
    return true;
  if (!lhs || !rhs || lhs->id != rhs->id ||
      lhs->display_text != rhs->display_text ||
      lhs->query_text != rhs->query_text ||
      lhs->accessibility_text != rhs->accessibility_text ||
      lhs->image_metadatas.size() != rhs->image_metadatas.size() ||
      lhs->sub_tiles.size() != rhs->sub_tiles.size())
    return false;

  for (const auto& it : lhs->image_metadatas) {
    auto found =
        std::find_if(rhs->image_metadatas.begin(), rhs->image_metadatas.end(),
                     [it](const ImageMetadata& image) { return image == it; });
    if (found == rhs->image_metadatas.end())
      return false;
  }

  for (auto& it : lhs->sub_tiles) {
    auto* target = it.get();
    auto found =
        std::find_if(rhs->sub_tiles.begin(), rhs->sub_tiles.end(),
                     [&target](const std::unique_ptr<QueryTileEntry>& entry) {
                       return entry->id == target->id;
                     });
    if (found == rhs->sub_tiles.end() ||
        !AreTreesIdentical(target, found->get()))
      return false;
  }
  return true;
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

QueryTileEntry::QueryTileEntry() = default;

QueryTileEntry::~QueryTileEntry() = default;

QueryTileEntry::QueryTileEntry(const QueryTileEntry& other) {
  DeepCopyTiles(&other, this);
}

QueryTileEntry::QueryTileEntry(QueryTileEntry&& other) {
  id = std::move(other.id);
  query_text = std::move(other.query_text);
  display_text = std::move(other.display_text);
  accessibility_text = std::move(other.accessibility_text);
  image_metadatas = std::move(other.image_metadatas);
  sub_tiles = std::move(other.sub_tiles);
}

bool QueryTileEntry::operator==(const QueryTileEntry& other) const {
  return AreTreesIdentical(this, &other);
}
bool QueryTileEntry::operator!=(const QueryTileEntry& other) const {
  return !(*this == other);
}

}  // namespace upboarding
