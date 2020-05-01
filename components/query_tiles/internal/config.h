// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_CONFIG_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_CONFIG_H_

#include <memory>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "url/gurl.h"

namespace upboarding {

struct TileConfig {
  // Creates a default TileConfig.
  static std::unique_ptr<TileConfig> Create();

  // Creates a TileConfig that reads parameters from Finch.
  static std::unique_ptr<TileConfig> CreateFromFinch();

  TileConfig();
  ~TileConfig();
  TileConfig(const TileConfig& other) = delete;
  TileConfig& operator=(const TileConfig& other) = delete;

  // Flag to tell whether query tiles is enabled or not.
  bool is_enabled;

  // The base URL for the Query Tiles server.
  GURL base_url;

  // The URL for GetQueryTiles RPC.
  GURL get_query_tile_url;

  // The maximum duration for holding current group's info and images.
  base::TimeDelta expire_duration;

  // Locale setting from operating system.
  std::string locale;

  // Flag to tell whether running the background task requires unmeter network
  // condition.
  bool is_unmetered_network_required;
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_CONFIG_H_
