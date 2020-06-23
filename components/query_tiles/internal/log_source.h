// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_LOG_SOURCE_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_LOG_SOURCE_H_

#include <string>

#include "base/observer_list.h"
#include "components/query_tiles/internal/log_sink.h"
#include "components/query_tiles/internal/tile_types.h"
#include "components/query_tiles/logger.h"
#include "net/base/backoff_entry.h"

namespace query_tiles {

// A source for all relevant logging data.  LoggerImpl will pull from an
// instance of LogSource to push relevant log information to observers.
class LogSource {
 public:
  virtual ~LogSource() = default;

  // Returns the TileFetcher status.
  virtual TileInfoRequestStatus GetFetcherStatus() = 0;

  // Returns the TileManager status.
  virtual TileGroupStatus GetGroupStatus() = 0;

  // Returns a serialized string of group info.
  virtual std::string GetGroupInfo() = 0;

  // Returns formatted string of tiles proto data.
  virtual std::string GetTilesProto() = 0;
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_LOG_SOURCE_H_
