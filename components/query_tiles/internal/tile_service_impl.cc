// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_service_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/rand_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/query_tiles/internal/proto_conversion.h"
#include "components/query_tiles/internal/tile_config.h"
#include "components/query_tiles/switches.h"

namespace query_tiles {
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

void OnInstantFetchComplete(bool success) {}

}  // namespace

TileServiceImpl::TileServiceImpl(
    std::unique_ptr<ImagePrefetcher> image_prefetcher,
    std::unique_ptr<TileManager> tile_manager,
    background_task::BackgroundTaskScheduler* scheduler,
    std::unique_ptr<TileFetcher> tile_fetcher,
    base::Clock* clock)
    : image_prefetcher_(std::move(image_prefetcher)),
      tile_manager_(std::move(tile_manager)),
      scheduler_(scheduler),
      tile_fetcher_(std::move(tile_fetcher)),
      clock_(clock) {
  ScheduleDailyTask();
}

TileServiceImpl::~TileServiceImpl() = default;

void TileServiceImpl::Initialize(SuccessCallback callback) {
  tile_manager_->Init(base::BindOnce(&TileServiceImpl::OnTileManagerInitialized,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback)));
}

void TileServiceImpl::OnTileManagerInitialized(SuccessCallback callback,
                                               TileGroupStatus status) {
  bool success = (status == TileGroupStatus::kSuccess);
  DCHECK(callback);
  // TODO(xingliu): Handle TileGroupStatus::kInvalidGroup.
  std::move(callback).Run(success);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kQueryTilesInstantBackgroundTask)) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&TileServiceImpl::StartFetchForTiles,
                       weak_ptr_factory_.GetWeakPtr(),
                       false /*is_from_reduced_mode*/,
                       base::BindOnce(&OnInstantFetchComplete)),
        base::TimeDelta::FromSeconds(2));
  }
}

void TileServiceImpl::GetQueryTiles(GetTilesCallback callback) {
  tile_manager_->GetTiles(std::move(callback));
}

void TileServiceImpl::GetTile(const std::string& tile_id,
                              TileCallback callback) {
  tile_manager_->GetTile(tile_id, std::move(callback));
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
      TileConfig::GetIsUnMeteredNetworkRequired()
          ? background_task::TaskInfo::NetworkType::UNMETERED
          : background_task::TaskInfo::NetworkType::ANY;

  scheduler_->Schedule(task_info);
}

void TileServiceImpl::StartFetchForTiles(
    bool is_from_reduced_mode,
    BackgroundTaskFinishedCallback task_finished_callback) {
  DCHECK(tile_fetcher_);
  tile_fetcher_->StartFetchForTiles(base::BindOnce(
      &TileServiceImpl::OnFetchFinished, weak_ptr_factory_.GetWeakPtr(),
      is_from_reduced_mode, std::move(task_finished_callback)));
}

// TODO(crbug.com/1077173): Handle the failures, retry mechanism and
// related metrics.
void TileServiceImpl::OnFetchFinished(
    bool is_from_reduced_mode,
    BackgroundTaskFinishedCallback task_finished_callback,
    TileInfoRequestStatus status,
    const std::unique_ptr<std::string> response_body) {
  query_tiles::proto::ServerResponse response_proto;
  if (status == TileInfoRequestStatus::kSuccess) {
    bool parse_success = response_proto.ParseFromString(*response_body.get());
    if (parse_success) {
      TileGroup group;
      TileGroupFromResponse(response_proto, &group);
      group.id = base::GenerateGUID();
      group.last_updated_ts = clock_->Now();
      auto group_copy = std::make_unique<TileGroup>(group);
      tile_manager_->SaveTiles(
          std::move(group_copy),
          base::BindOnce(&TileServiceImpl::OnTilesSaved,
                         weak_ptr_factory_.GetWeakPtr(), std::move(group),
                         is_from_reduced_mode,
                         std::move(task_finished_callback)));
    }
  } else {
    std::move(task_finished_callback).Run(false /*reschedule*/);
  }
}

void TileServiceImpl::OnTilesSaved(
    TileGroup tile_group,
    bool is_from_reduced_mode,
    BackgroundTaskFinishedCallback task_finished_callback,
    TileGroupStatus status) {
  if (status != TileGroupStatus::kSuccess) {
    std::move(task_finished_callback).Run(false /*reschedule*/);
    return;
  }

  image_prefetcher_->Prefetch(
      std::move(tile_group), is_from_reduced_mode,
      base::BindOnce(&TileServiceImpl::OnPrefetchImagesDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(task_finished_callback)));
}

void TileServiceImpl::OnPrefetchImagesDone(
    BackgroundTaskFinishedCallback task_finished_callback) {
  DCHECK(task_finished_callback);
  std::move(task_finished_callback).Run(false /*reschedule*/);
}

}  // namespace query_tiles
