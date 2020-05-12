// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_CONFIG_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_CONFIG_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "components/query_tiles/internal/tile_types.h"
#include "url/gurl.h"

namespace upboarding {

// Default URL string for GetQueryTiles RPC.
extern const char kDefaultGetQueryTilePath[];

// Finch parameter key for experiment tag to be passed to the server.
extern const char kExperimentTagKey[];

// Finch parameter key for base server URL to retrieve the tiles.
extern const char kBaseURLKey[];

// Finch parameter key for expire duration in seconds.
extern const char kExpireDurationKey[];

// Finch parameter key for expire duration in seconds.
extern const char kIsUnmeteredNetworkRequiredKey[];

// Finch parameter key for image prefetch mode.
extern const char kImagePrefetchModeKey[];

class TileConfig {
 public:
  // Gets the URL for the Query Tiles server.
  static GURL GetQueryTilesServerUrl();

  // Gets whether running the background task requires unmeter network
  // condition.
  static bool GetIsUnMeteredNetworkRequired();

  // Gets the experiment tag to be passed to server.
  static std::string GetExperimentTag();

  // Gets the maximum duration for holding current group's info and images.
  static base::TimeDelta GetExpireDuration();

  // Gets the image prefetch mode to determine how many images will be
  // prefetched in background task.
  static ImagePrefetchMode GetImagePrefetchMode();
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_CONFIG_H_
