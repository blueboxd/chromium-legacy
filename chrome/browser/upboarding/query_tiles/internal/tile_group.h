// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_GROUP_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_GROUP_H_

#include <memory>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/upboarding/query_tiles/query_tile_entry.h"

namespace upboarding {

// A group of query tiles and metadata.
struct TileGroup {
  TileGroup();
  ~TileGroup();

  bool operator==(const TileGroup& other) const;
  bool operator!=(const TileGroup& other) const;

  TileGroup(const TileGroup& other);
  TileGroup(TileGroup&& other);

  // Unique id for the group.
  std::string id;

  // Locale setting of this group.
  std::string locale;

  // Last updated timestamp.
  base::Time last_updated_ts;

  // Top level tiles.
  std::vector<std::unique_ptr<QueryTileEntry>> tiles;
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_GROUP_H_
