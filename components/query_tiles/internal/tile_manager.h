// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_MANAGER_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "base/time/clock.h"
#include "components/query_tiles/internal/store.h"
#include "components/query_tiles/internal/tile_group.h"
#include "components/query_tiles/internal/tile_types.h"
#include "components/query_tiles/tile.h"

namespace upboarding {

// Manages the in-memory tile group and coordinates with storage layer.
class TileManager {
 public:
  using TileStore = Store<TileGroup>;
  using TileGroupStatusCallback = base::OnceCallback<void(TileGroupStatus)>;
  using GetTilesCallback = base::OnceCallback<void(std::vector<Tile>)>;
  using TileCallback = base::OnceCallback<void(base::Optional<Tile>)>;

  // Creates the instance.
  static std::unique_ptr<TileManager> Create(
      std::unique_ptr<TileStore> tile_store,
      base::Clock* clock);

  // Initializes the query tile store, loading them into memory after
  // validating.
  virtual void Init(TileGroupStatusCallback callback) = 0;

  // Returns tiles to the caller.
  virtual void GetTiles(GetTilesCallback callback) = 0;

  // Returns the tile associated with |tile_id| to the caller.
  virtual void GetTile(const std::string& tile_id, TileCallback callback) = 0;

  // Save the query tiles into database.
  virtual void SaveTiles(std::unique_ptr<TileGroup> tile_group,
                         TileGroupStatusCallback callback) = 0;

  TileManager();
  virtual ~TileManager() = default;

  TileManager(const TileManager& other) = delete;
  TileManager& operator=(const TileManager& other) = delete;
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_MANAGER_H_
