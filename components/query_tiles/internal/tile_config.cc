// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_config.h"

#include "base/metrics/field_trial_params.h"
#include "components/query_tiles/switches.h"

namespace upboarding {

// Default base URL string for the Query Tiles server.
constexpr char kDefaultBaseURL[] =
    "https://autopush-gsaprototype-pa.sandbox.googleapis.com";

// Default URL string for GetQueryTiles RPC.
const char kDefaultGetQueryTilePath[] = "/v1/querytiles";

// Finch parameter key for experiment tag to be passed to the server.
const char kExperimentTagKey[] = "experiment_tag";

// Finch parameter key for base server URL to retrieve the tiles.
const char kBaseURLKey[] = "base_url";

// Finch parameter key for expire duration in seconds.
const char kExpireDurationKey[] = "expire_duration";

// Finch parameter key for expire duration in seconds.
const char kIsUnmeteredNetworkRequiredKey[] = "is_unmetered_network_required";

// Default expire duration.
const int kDefaultExpireDurationInSeconds = 48 * 60 * 60;

namespace {
const GURL BuildGetQueryTileURL(const GURL& base_url, const char* path) {
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return base_url.ReplaceComponents(replacements);
}
}  // namespace

// static
GURL TileConfig::GetQueryTilesServerUrl() {
  std::string base_url = base::GetFieldTrialParamValueByFeature(
      features::kQueryTiles, kBaseURLKey);
  GURL server_url = base_url.empty() ? GURL(kDefaultBaseURL) : GURL(base_url);
  return BuildGetQueryTileURL(server_url, kDefaultGetQueryTilePath);
}

// static
bool TileConfig::GetIsUnMeteredNetworkRequired() {
  return base::GetFieldTrialParamByFeatureAsBool(
      features::kQueryTiles, kIsUnmeteredNetworkRequiredKey, false);
}

// static
std::string TileConfig::GetExperimentTag() {
  return base::GetFieldTrialParamValueByFeature(features::kQueryTiles,
                                                kExperimentTagKey);
}

// static
base::TimeDelta TileConfig::GetExpireDuration() {
  int time_in_seconds = base::GetFieldTrialParamByFeatureAsInt(
      features::kQueryTiles, kExpireDurationKey,
      kDefaultExpireDurationInSeconds);
  return base::TimeDelta::FromSeconds(time_in_seconds);
}

}  // namespace upboarding
