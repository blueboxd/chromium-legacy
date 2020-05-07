// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/init_aware_tile_service.h"

#include "base/bind.h"

namespace upboarding {

InitAwareTileService::InitAwareTileService(
    std::unique_ptr<InitializableTileService> tile_service)
    : tile_service_(std::move(tile_service)) {
  tile_service_->Initialize(
      base::BindOnce(&InitAwareTileService::OnTileServiceInitialized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InitAwareTileService::OnTileServiceInitialized(bool success) {
  DCHECK(!init_success_.has_value());
  init_success_ = success;

  // Drops all api calls when initialization failed.
  // TODO(xingliu): Provides a backup empty callback for GetQueryTiles().
  if (!success) {
    cached_api_calls_.clear();
    return;
  }

  // Flush all cached calls in FIFO sequence.
  while (!cached_api_calls_.empty()) {
    auto api_call = std::move(cached_api_calls_.front());
    cached_api_calls_.pop_front();
    std::move(api_call).Run();
  }
}

void InitAwareTileService::GetQueryTiles(GetTilesCallback callback) {
  if (IsReady()) {
    tile_service_->GetQueryTiles(std::move(callback));
    return;
  }

  MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::GetQueryTiles,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(callback)));
}

void InitAwareTileService::GetTile(const std::string& tile_id,
                                   TileCallback callback) {
  if (IsReady()) {
    tile_service_->GetTile(tile_id, std::move(callback));
    return;
  }

  MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::GetTile,
                                   weak_ptr_factory_.GetWeakPtr(), tile_id,
                                   std::move(callback)));
}

void InitAwareTileService::StartFetchForTiles(
    BackgroundTaskFinishedCallback callback) {
  if (IsReady()) {
    tile_service_->StartFetchForTiles(std::move(callback));
    return;
  }

  MaybeCacheApiCall(base::BindOnce(&InitAwareTileService::StartFetchForTiles,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(callback)));
}

void InitAwareTileService::MaybeCacheApiCall(base::OnceClosure api_call) {
  // If initialization failed, discard the |closure|.
  if (init_success_.has_value() && !init_success_.value())
    return;

  // Cache |api_call| if not initialized.
  if (!init_success_.has_value())
    cached_api_calls_.emplace_back(std::move(api_call));
}

bool InitAwareTileService::IsReady() const {
  return init_success_.has_value() && init_success_.value();
}

InitAwareTileService::~InitAwareTileService() = default;

}  // namespace upboarding
