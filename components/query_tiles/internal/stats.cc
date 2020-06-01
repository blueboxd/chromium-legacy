// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/stats.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"

namespace query_tiles {
namespace stats {

const char kImagePreloadingHistogram[] =
    "Search.QueryTiles.ImagePreloadingEvent";

const char kHttpResponseCodeHistogram[] =
    "Search.QueryTiles.FetcherHttpResponseCode";

const char kNetErrorCodeHistogram[] = "Search.QueryTiles.FetcherNetErrorCode";

const char kRequestStatusHistogram[] = "Search.QueryTiles.RequestStatus";

const char kGroupStatusHistogram[] = "Search.QueryTiles.GroupStatus";

const char kFirstFlowDurationHistogram[] =
    "Search.QueryTiles.Fetcher.FirstFlowDuration";

const char kFetcherStartHourHistogram[] = "Search.QueryTiles.Fetcher.Start";

void RecordImageLoading(ImagePreloadingEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kImagePreloadingHistogram, event);
}

void RecordTileFetcherResponseCode(int response_code) {
  base::UmaHistogramSparse(kHttpResponseCodeHistogram, response_code);
}

void RecordTileFetcherNetErrorCode(int error_code) {
  base::UmaHistogramSparse(kNetErrorCodeHistogram, -error_code);
}

void RecordTileRequestStatus(TileInfoRequestStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kRequestStatusHistogram, status);
}

void RecordTileGroupStatus(TileGroupStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kGroupStatusHistogram, status);
}

void RecordFirstFetchFlowDuration(int hours) {
  UMA_HISTOGRAM_COUNTS_100(kFirstFlowDurationHistogram, hours);
}

void RecordExplodeOnFetchStarted(int explode_hour) {
  UMA_HISTOGRAM_EXACT_LINEAR(kFetcherStartHourHistogram, explode_hour, 24);
}

}  // namespace stats
}  // namespace query_tiles
