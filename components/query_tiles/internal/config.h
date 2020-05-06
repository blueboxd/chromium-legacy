// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_CONFIG_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_CONFIG_H_

#include <memory>
#include <string>

#include "base/time/time.h"
#include "url/gurl.h"

namespace upboarding {

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
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_CONFIG_H_
