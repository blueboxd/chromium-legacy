// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_H_

#include <memory>

#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/internal/tile_types.h"
#include "components/query_tiles/tile_service_prefs.h"
#include "net/base/backoff_entry_serializer.h"

class PrefService;

namespace query_tiles {

// Coordinates with native background task scheduler to schedule or cancel a
// TileBackgroundTask.
class TileServiceScheduler {
 public:
  // Method to create a TileServiceScheduler instance.
  static std::unique_ptr<TileServiceScheduler> Create(
      background_task::BackgroundTaskScheduler* scheduler,
      PrefService* prefs,
      base::Clock* clock,
      const base::TickClock* tick_clock,
      std::unique_ptr<net::BackoffEntry::Policy> backoff_policy);

  // Called on fetch task completed, schedule another task with or without
  // backoff based on the status. Success status will lead a regular schedule
  // after around 14 - 18 hours. Failure status will lead a backoff, the release
  // duration is related to count of failures. Suspend status will directly set
  // the release time until 24 hours later.
  virtual void OnFetchCompleted(TileInfoRequestStatus status) = 0;

  // Called on tile manager initialization completed, schedule another task with
  // or without backoff based on the status. NoTiles status will lead a regular
  // schedule after around 14 - 18 hours. DbOperationFailure status will
  // directly set the release time until 24 hours later.
  virtual void OnTileManagerInitialized(TileGroupStatus status) = 0;

  // Cancel current existing task, and reset backoff.
  virtual void CancelTask() = 0;

  virtual ~TileServiceScheduler();

  TileServiceScheduler(const TileServiceScheduler& other) = delete;
  TileServiceScheduler& operator=(const TileServiceScheduler& other) = delete;

 protected:
  TileServiceScheduler();
};

}  // namespace query_tiles

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_SCHEDULER_H_
