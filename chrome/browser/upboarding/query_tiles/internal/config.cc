// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/config.h"

namespace upboarding {
namespace {

// Default base URL string for the Query Tiles server.
constexpr char kDefaultBaseURL[] =
    "https://autopush-gsaprototype-pa.sandbox.googleapis.com";

// Default URL string for GetQueryTiles RPC.
constexpr char kDefaultGetQueryTilePath[] = "/v1/querytiles";

// Default state of QueryTile feature.
constexpr bool kDefaultQueryTileState = false;

const GURL BuildGetQueryTileURL(const GURL& base_url, const char* path) {
  GURL::Replacements replacements;
  replacements.SetPathStr(path);
  return base_url.ReplaceComponents(replacements);
}

}  // namespace

std::unique_ptr<QueryTilesConfig> QueryTilesConfig::Create() {
  return std::make_unique<QueryTilesConfig>();
}

std::unique_ptr<QueryTilesConfig> QueryTilesConfig::CreateFromFinch() {
  // TODO(hesen): Implement reading parameters from Finch.
  return std::make_unique<QueryTilesConfig>();
}

QueryTilesConfig::QueryTilesConfig()
    : is_enabled(kDefaultQueryTileState),
      base_url(GURL(kDefaultBaseURL)),
      get_query_tile_url(
          BuildGetQueryTileURL(base_url, kDefaultGetQueryTilePath)) {}

QueryTilesConfig::~QueryTilesConfig() = default;

}  // namespace upboarding
