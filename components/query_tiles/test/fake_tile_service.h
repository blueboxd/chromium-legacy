// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_TEST_FAKE_TILE_SERVICE_H_
#define COMPONENTS_QUERY_TILES_TEST_FAKE_TILE_SERVICE_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/query_tiles/tile_service.h"

namespace upboarding {

// A fake TileService that can be used for tests.
class FakeTileService : public TileService {
 public:
  FakeTileService();
  ~FakeTileService() override;

  // Disallow copy/assign.
  FakeTileService(const FakeTileService& other) = delete;
  FakeTileService& operator=(const FakeTileService& other) = delete;

 private:
  // TileService implementation.
  void GetQueryTiles(GetTilesCallback callback) override;
  void GetTile(const std::string& tile_id, TileCallback callback) override;
  void StartFetchForTiles(BackgroundTaskFinishedCallback callback) override;

  std::vector<std::unique_ptr<Tile>> tiles_;
  base::WeakPtrFactory<FakeTileService> weak_ptr_factory_{this};
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_TEST_FAKE_TILE_SERVICE_H_
