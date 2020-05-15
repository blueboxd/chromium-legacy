// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_STATS_H_

#include "components/query_tiles/internal/tile_types.h"

namespace query_tiles {
namespace stats {

// Event to track image loading metrics.
enum class ImageLoadingEvent {
  // Starts to fetch image in reduced mode background task.
  kStartPrefetch = 0,
  kPrefetchSuccess = 1,
  kPrefetchFailure = 2,
  kMaxValue = kPrefetchFailure,
};

// Records an image loading event.
void RecordImageLoading(ImageLoadingEvent event);

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
