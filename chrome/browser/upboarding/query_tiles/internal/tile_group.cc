// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/tile_group.h"

namespace upboarding {

TileGroup::TileGroup() = default;

TileGroup::~TileGroup() = default;

TileGroup::TileGroup(const TileGroup& other) {
  id = other.id;
  locale = other.locale;
  last_updated_ts = other.last_updated_ts;
  for (const auto& tile : other.tiles) {
    std::unique_ptr<QueryTileEntry> copy =
        std::make_unique<QueryTileEntry>((*tile.get()));
    tiles.emplace_back(std::move(copy));
  }
}

TileGroup::TileGroup(TileGroup&& other) {
  id = std::move(other.id);
  locale = std::move(other.locale);
  last_updated_ts = std::move(other.last_updated_ts);
  other.last_updated_ts = base::Time();
  tiles = std::move(other.tiles);
}

bool TileGroup::operator==(const TileGroup& other) const {
  if (id != other.id || locale != other.locale ||
      last_updated_ts != other.last_updated_ts ||
      tiles.size() != other.tiles.size())
    return false;

  for (const auto& it : tiles) {
    auto* target = it.get();
    auto found =
        std::find_if(other.tiles.begin(), other.tiles.end(),
                     [&target](const std::unique_ptr<QueryTileEntry>& entry) {
                       return entry->id == target->id;
                     });
    if (found == other.tiles.end() || *target != *found->get())
      return false;
  }

  return true;
}

bool TileGroup::operator!=(const TileGroup& other) const {
  return !(*this == other);
}

}  // namespace upboarding
