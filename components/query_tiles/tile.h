// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_TILE_H_
#define COMPONENTS_QUERY_TILES_TILE_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

namespace upboarding {

// Metadata of a tile image.
struct ImageMetadata {
  ImageMetadata();
  explicit ImageMetadata(const GURL& url);
  ~ImageMetadata();
  ImageMetadata(const ImageMetadata& other);
  bool operator==(const ImageMetadata& other) const;

  // URL of the image.
  GURL url;
};

// Represents the in memory structure of Tile.
struct Tile {
  Tile();
  ~Tile();
  bool operator==(const Tile& other) const;
  bool operator!=(const Tile& other) const;

  Tile(const Tile& other);
  Tile(Tile&& other) noexcept;

  Tile& operator=(const Tile& other);
  Tile& operator=(Tile&& other) noexcept;

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

  // A list of children of this tile.
  std::vector<std::unique_ptr<Tile>> sub_tiles;
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_TILE_H_
