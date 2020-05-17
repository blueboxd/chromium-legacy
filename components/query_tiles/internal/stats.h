// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_

#include "components/query_tiles/internal/tile_types.h"

namespace query_tiles {
namespace stats {

// Event to track image loading metrics.
enum class ImagePreloadingEvent {
  // Start to fetch image in full browser mode.
  kStart = 0,
  // Fetch success in full browser mode.
  kSuccess = 1,
  // Fetch failure in full browser mode.
  kFailure = 2,
  // Start to fetch image in reduced mode.
  kStartReducedMode = 3,
  // Fetch success in reduced mode.
  kSuccessReducedMode = 4,
  // Fetch failure in reduced mode.
  kFailureReducedMode = 5,
  kMaxValue = kFailureReducedMode,
};

// Records an image preloading event.
void RecordImageLoading(ImagePreloadingEvent event);

// Records HTTP response code.
void RecordTileFetcherResponseCode(int response_code);

// Records net error code.
void RecordTileFetcherNetErrorCode(int error_code);

// Records request result from tile fetcher.
void RecordTileRequestStatus(TileInfoRequestStatus status);

// Records status of tile group.
void RecordTileGroupStatus(TileGroupStatus status);

}  // namespace stats
}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_
