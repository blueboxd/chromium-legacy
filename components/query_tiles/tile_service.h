// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_TILE_SERVICE_H_
#define COMPONENTS_QUERY_TILES_TILE_SERVICE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/optional.h"
#include "base/supports_user_data.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/query_tiles/tile.h"

namespace gfx {
class Image;
}  // namespace gfx

namespace upboarding {

using TileList = std::vector<Tile>;
using GetTilesCallback = base::OnceCallback<void(TileList)>;
using TileCallback = base::OnceCallback<void(base::Optional<Tile>)>;
using VisualsCallback = base::OnceCallback<void(const gfx::Image&)>;
using BackgroundTaskFinishedCallback = base::OnceCallback<void(bool)>;

// The central class on chrome client responsible for fetching, storing,
// managing, and displaying query tiles in chrome.
class TileService : public KeyedService, public base::SupportsUserData {
 public:
  // Called to retrieve all the tiles.
  virtual void GetQueryTiles(GetTilesCallback callback) = 0;

  // Called to retrieve the tile associated with |tile_id|.
  virtual void GetTile(const std::string& tile_id, TileCallback callback) = 0;

  // Start fetch query tiles from server.
  virtual void StartFetchForTiles(BackgroundTaskFinishedCallback callback) = 0;

  TileService() = default;
  ~TileService() override = default;

  TileService(const TileService&) = delete;
  TileService& operator=(const TileService&) = delete;
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_TILE_SERVICE_H_
