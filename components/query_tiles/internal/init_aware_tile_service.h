// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_INIT_AWARE_TILE_SERVICE_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_INIT_AWARE_TILE_SERVICE_H_

#include <deque>
#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "components/query_tiles/internal/tile_service_impl.h"
#include "components/query_tiles/tile_service.h"

namespace upboarding {

// TileService that can cache API calls before the underlying |tile_service_| is
// initialized. After a successful initialization, all cached API calls will be
// flushed in sequence.
class InitAwareTileService : public TileService {
 public:
  explicit InitAwareTileService(
      std::unique_ptr<InitializableTileService> tile_service);
  ~InitAwareTileService() override;

 private:
  // TileService implementation.
  void GetQueryTiles(GetTilesCallback callback) override;
  void GetTile(const std::string& tile_id, TileCallback callback) override;
  void StartFetchForTiles(BackgroundTaskFinishedCallback callback) override;

  void OnTileServiceInitialized(bool success);
  void MaybeCacheApiCall(base::OnceClosure api_call);

  // Returns whether |tile_service_| is successfully initialized.
  bool IsReady() const;

  std::unique_ptr<InitializableTileService> tile_service_;
  std::deque<base::OnceClosure> cached_api_calls_;

  // The initialization result of |tile_service_|.
  base::Optional<bool> init_success_;

  base::WeakPtrFactory<InitAwareTileService> weak_ptr_factory_{this};
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_INIT_AWARE_TILE_SERVICE_H_
