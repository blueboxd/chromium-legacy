// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/query_tiles/internal/config.h"
#include "components/query_tiles/internal/image_loader.h"
#include "components/query_tiles/internal/tile_manager.h"
#include "components/query_tiles/tile_service.h"

namespace upboarding {

class TileServiceImpl : public TileService {
 public:
  TileServiceImpl(std::unique_ptr<ImageLoader> image_loader,
                  std::unique_ptr<TileManager> tile_manager,
                  std::unique_ptr<TileConfig> config,
                  background_task::BackgroundTaskScheduler* scheduler);
  ~TileServiceImpl() override;

  // Disallow copy/assign.
  TileServiceImpl(const TileServiceImpl& other) = delete;
  TileServiceImpl& operator=(const TileServiceImpl& other) = delete;

 private:
  // TileService implementation.
  void GetQueryTiles(GetTilesCallback callback) override;
  void GetTile(const std::string& tile_id, TileCallback callback) override;
  void GetVisuals(const std::string& tile_id,
                  VisualsCallback callback) override;

  // TODO(hesen): Use an one-off task solution instead of periodic task.
  // Schedules periodic background task to start fetch.
  void ScheduleDailyTask();

  // Used to load tile images.
  std::unique_ptr<ImageLoader> image_loader_;

  // Manages in memory tile group and coordinates with TileStore.
  std::unique_ptr<TileManager> tile_manager_;

  // Config for the feature.
  std::unique_ptr<TileConfig> config_;

  // Background task scheduler, obtained from native
  // BackgroundTaskSchedulerFactory.
  background_task::BackgroundTaskScheduler* scheduler_;

  base::WeakPtrFactory<TileServiceImpl> weak_ptr_factory_{this};
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_
