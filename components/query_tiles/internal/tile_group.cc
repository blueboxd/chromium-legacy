// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_group.h"

#include <utility>

namespace upboarding {

namespace {

void DeepCopyGroup(const TileGroup& input, TileGroup* output) {
  DCHECK(output);

  output->id = input.id;
  output->locale = input.locale;
  output->last_updated_ts = input.last_updated_ts;
  output->tiles.clear();
  for (const auto& tile : input.tiles)
    output->tiles.emplace_back(std::make_unique<Tile>(*tile.get()));
}

}  // namespace

TileGroup::TileGroup() = default;

TileGroup::~TileGroup() = default;

bool TileGroup::operator==(const TileGroup& other) const {
  return id == other.id && locale == other.locale &&
         last_updated_ts == other.last_updated_ts &&
         tiles.size() == other.tiles.size();
}

bool TileGroup::operator!=(const TileGroup& other) const {
  return !(*this == other);
}

TileGroup::TileGroup(const TileGroup& other) {
  DeepCopyGroup(other, this);
}

TileGroup::TileGroup(TileGroup&& other) = default;

TileGroup& TileGroup::operator=(const TileGroup& other) {
  DeepCopyGroup(other, this);
  return *this;
}

TileGroup& TileGroup::operator=(TileGroup&& other) = default;

}  // namespace upboarding
