// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_QUERY_TILE_ENTRY_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_QUERY_TILE_ENTRY_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "url/gurl.h"

namespace upboarding {

// Metadata of an query tile image.
struct ImageMetadata {
  ImageMetadata();
  ImageMetadata(const std::string& id, const GURL& url);
  ~ImageMetadata();
  ImageMetadata(const ImageMetadata& other);
  bool operator==(const ImageMetadata& other) const;

  // Unique Id for image.
  std::string id;

  // Origin URL the image fetched from.
  GURL url;
};

// Represents the in memory structure of QueryTile.
struct QueryTileEntry {
  QueryTileEntry();
  ~QueryTileEntry();
  QueryTileEntry(const QueryTileEntry& other);
  bool operator==(const QueryTileEntry& other) const;

  // Unique Id for each entry.
  std::string id;

  // String of query that send to the search engine.
  std::string query_text;

  // String of the text that displays in UI.
  std::string display_text;

  // Text for accessibility purposes, in pair with |display_text|.
  std::string accessibility_text;

  // A list of images's matadatas.
  std::vector<ImageMetadata> image_metadatas;

  // A set contains all ids of it's children tiles.
  std::set<std::string> children;
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_QUERY_TILE_ENTRY_H_
