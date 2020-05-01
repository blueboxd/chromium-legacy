// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_service_impl.h"

#include <string>
#include <utility>

#include "base/rand_util.h"

namespace upboarding {
namespace {

// Default periodic interval of background task.
constexpr base::TimeDelta kBackgroundTaskInterval =
    base::TimeDelta::FromHours(16);

// Default length of random window added to the periodic interval.
constexpr base::TimeDelta kBackgroundTaskRandomWindow =
    base::TimeDelta::FromHours(6);

// Default flex time of background task.
constexpr base::TimeDelta kBackgroundTaskFlexTime =
    base::TimeDelta::FromHours(2);

}  // namespace

TileServiceImpl::TileServiceImpl(
    std::unique_ptr<ImageLoader> image_loader,
    std::unique_ptr<TileManager> tile_manager,
    std::unique_ptr<TileConfig> config,
    background_task::BackgroundTaskScheduler* scheduler,
    std::unique_ptr<TileFetcher> tile_fetcher)
    : image_loader_(std::move(image_loader)),
      tile_manager_(std::move(tile_manager)),
      config_(std::move(config)),
      scheduler_(scheduler),
      tile_fetcher_(std::move(tile_fetcher)) {
  ScheduleDailyTask();
}

TileServiceImpl::~TileServiceImpl() = default;

void TileServiceImpl::GetQueryTiles(GetTilesCallback callback) {
  tile_manager_->GetTiles(std::move(callback));
}

void TileServiceImpl::GetTile(const std::string& tile_id,
                              TileCallback callback) {
  tile_manager_->GetTile(tile_id, std::move(callback));
}

void TileServiceImpl::GetVisuals(const std::string& tile_id,
                                 VisualsCallback callback) {
  NOTIMPLEMENTED();
}

void TileServiceImpl::ScheduleDailyTask() {
  background_task::PeriodicInfo periodic_info;
  periodic_info.interval_ms =
      kBackgroundTaskInterval.InMilliseconds() +
      base::RandGenerator(kBackgroundTaskRandomWindow.InMilliseconds());
  periodic_info.flex_ms = kBackgroundTaskFlexTime.InMilliseconds();

  background_task::TaskInfo task_info(
      static_cast<int>(background_task::TaskIds::QUERY_TILE_JOB_ID),
      periodic_info);
  task_info.is_persisted = true;
  task_info.update_current = false;
  task_info.network_type =
      config_->is_unmetered_network_required
          ? background_task::TaskInfo::NetworkType::UNMETERED
          : background_task::TaskInfo::NetworkType::ANY;

  scheduler_->Schedule(task_info);
}

}  // namespace upboarding
