// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/tile.h"

#include <utility>

namespace upboarding {
namespace {

void DeepCopyTiles(const Tile& input, Tile* out) {
  DCHECK(out);

  out->id = input.id;
  out->display_text = input.display_text;
  out->query_text = input.query_text;
  out->accessibility_text = input.accessibility_text;
  out->image_metadatas = input.image_metadatas;
  out->sub_tiles.clear();
  for (const auto& child : input.sub_tiles) {
    auto entry = std::make_unique<Tile>();
    DeepCopyTiles(*child.get(), entry.get());
    out->sub_tiles.emplace_back(std::move(entry));
  }
}

}  // namespace

ImageMetadata::ImageMetadata() = default;

ImageMetadata::ImageMetadata(const GURL& url) : url(url) {}

ImageMetadata::~ImageMetadata() = default;

ImageMetadata::ImageMetadata(const ImageMetadata& other) = default;

bool ImageMetadata::operator==(const ImageMetadata& other) const {
  return url == other.url;
}

bool Tile::operator==(const Tile& other) const {
  return id == other.id && display_text == other.display_text &&
         query_text == other.query_text &&
         accessibility_text == other.accessibility_text &&
         image_metadatas.size() == other.image_metadatas.size() &&
         sub_tiles.size() == other.sub_tiles.size();
}

bool Tile::operator!=(const Tile& other) const {
  return !(*this == other);
}

Tile::Tile(const Tile& other) {
  DeepCopyTiles(other, this);
}

Tile::Tile() = default;

Tile::Tile(Tile&& other) = default;

Tile::~Tile() = default;

Tile& Tile::operator=(const Tile& other) {
  DeepCopyTiles(other, this);
  return *this;
}

Tile& Tile::operator=(Tile&& other) = default;

}  // namespace upboarding
