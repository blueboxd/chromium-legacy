// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"

namespace query_tiles {
namespace stats {

void RecordImageLoading(ImageLoadingEvent event) {
  NOTIMPLEMENTED();
}

void RecordTileFetcherResponseCode(int response_code) {
  base::UmaHistogramSparse("Search.QueryTiles.FetcherHttpResponseCode",
                           response_code);
}

void RecordTileFetcherNetErrorCode(int error_code) {
  base::UmaHistogramSparse("Search.QueryTiles.FetcherNetErrorCode",
                           -error_code);
}

void RecordTileRequestStatus(TileInfoRequestStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Search.QueryTiles.RequestStatus", status);
}

void RecordTileGroupStatus(TileGroupStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Search.QueryTiles.GroupStatus", status);
}

}  // namespace stats
}  // namespace query_tiles
