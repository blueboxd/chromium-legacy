// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/query_tiles/internal/image_loader.h"
#include "components/query_tiles/internal/tile_fetcher.h"
#include "components/query_tiles/internal/tile_manager.h"
#include "components/query_tiles/internal/tile_types.h"
#include "components/query_tiles/tile_service.h"

namespace upboarding {

// A TileService that needs to be explicitly initialized.
class InitializableTileService : public TileService {
 public:
  // Initializes the tile service.
  virtual void Initialize(SuccessCallback callback) = 0;

  InitializableTileService() = default;
  ~InitializableTileService() override = default;
};

class TileServiceImpl : public InitializableTileService {
 public:
  TileServiceImpl(std::unique_ptr<ImageLoader> image_loader,
                  std::unique_ptr<TileManager> tile_manager,
                  background_task::BackgroundTaskScheduler* scheduler,
                  std::unique_ptr<TileFetcher> tile_fetcher,
                  base::Clock* clock);
  ~TileServiceImpl() override;

  // Disallow copy/assign.
  TileServiceImpl(const TileServiceImpl& other) = delete;
  TileServiceImpl& operator=(const TileServiceImpl& other) = delete;

 private:
  // InitializableTileService implementation.
  void Initialize(SuccessCallback callback) override;
  void GetQueryTiles(GetTilesCallback callback) override;
  void GetTile(const std::string& tile_id, TileCallback callback) override;
  void StartFetchForTiles(BackgroundTaskFinishedCallback callback) override;

  // Called when tile manager is initialized.
  void OnTileManagerInitialized(SuccessCallback callback,
                                TileGroupStatus status);
  // TODO(hesen): Use an one-off task solution instead of periodic task.
  // Schedules periodic background task to start fetch.
  void ScheduleDailyTask();

  // Called when fetching from server is completed.
  void OnFetchFinished(BackgroundTaskFinishedCallback task_finished_callback,
                       TileInfoRequestStatus status,
                       const std::unique_ptr<std::string> response_body);

  // Called when saving to db via manager layer is completed.
  void OnTilesSaved(BackgroundTaskFinishedCallback task_finished_callback,
                    TileGroupStatus status);

  // Used to load tile images.
  std::unique_ptr<ImageLoader> image_loader_;

  // Manages in memory tile group and coordinates with TileStore.
  std::unique_ptr<TileManager> tile_manager_;

  // Background task scheduler, obtained from native
  // BackgroundTaskSchedulerFactory.
  background_task::BackgroundTaskScheduler* scheduler_;

  // Fetcher to execute download jobs from Google server.
  std::unique_ptr<TileFetcher> tile_fetcher_;

  // Clock object.
  base::Clock* clock_;

  base::WeakPtrFactory<TileServiceImpl> weak_ptr_factory_{this};
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_
