// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/config.h"

#include "base/metrics/field_trial_params.h"
#include "components/query_tiles/switches.h"

namespace upboarding {
namespace {

// Default base URL string for the Query Tiles server.
constexpr char kDefaultBaseURL[] =
    "https://autopush-gsaprototype-pa.sandbox.googleapis.com";

// Default URL string for GetQueryTiles RPC.
constexpr char kDefaultGetQueryTilePath[] = "/v1/querytiles";

// Finch parameter key for experiment tag to be passed to the server.
constexpr char kExperimentTagKey[] = "experiment_tag";

// Default expire duration.
constexpr base::TimeDelta kDefaultExpireDuration =
    base::TimeDelta::FromHours(48);

// Default network requirement for background task.
constexpr bool kDefaultIsUnmeterNetworkRequired = false;

const GURL BuildGetQueryTileURL(const GURL& base_url, const char* path) {
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return base_url.ReplaceComponents(replacements);
}

}  // namespace

// static
GURL TileConfig::GetQueryTilesServerUrl() {
  GURL base_url(kDefaultBaseURL);
  return BuildGetQueryTileURL(base_url, kDefaultGetQueryTilePath);
}

// static
bool TileConfig::GetIsUnMeteredNetworkRequired() {
  return kDefaultIsUnmeterNetworkRequired;
}

// static
std::string TileConfig::GetExperimentTag() {
  return base::GetFieldTrialParamValueByFeature(features::kQueryTiles,
                                                kExperimentTagKey);
}

// static
base::TimeDelta TileConfig::GetExpireDuration() {
  return kDefaultExpireDuration;
}

}  // namespace upboarding
