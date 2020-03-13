// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/query_tile_entry.h"

namespace upboarding {

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

QueryTileEntry::QueryTileEntry(const QueryTileEntry& other) = default;

bool QueryTileEntry::operator==(const QueryTileEntry& other) const {
  return id == other.id && query_text == other.query_text &&
         display_text == other.display_text &&
         accessibility_text == other.accessibility_text &&
         image_metadatas.size() == other.image_metadatas.size() &&
         children.size() == other.children.size();
}

}  // namespace upboarding
