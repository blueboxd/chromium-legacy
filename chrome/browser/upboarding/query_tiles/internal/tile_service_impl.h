// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/upboarding/query_tiles/internal/image_loader.h"
#include "chrome/browser/upboarding/query_tiles/tile_service.h"

namespace upboarding {

class TileServiceImpl : public TileService {
 public:
  explicit TileServiceImpl(std::unique_ptr<ImageLoader> image_loader);
  ~TileServiceImpl() override;

  // Disallow copy/assign.
  TileServiceImpl(const TileServiceImpl& other) = delete;
  TileServiceImpl& operator=(const TileServiceImpl& other) = delete;

 private:
  // TileService implementation.
  void GetQueryTiles(GetTilesCallback callback) override;
  void GetVisuals(const std::string& tile_id,
                  VisualsCallback callback) override;

  // Used to load tile images.
  std::unique_ptr<ImageLoader> image_loader_;

  base::WeakPtrFactory<TileServiceImpl> weak_ptr_factory_{this};
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_SERVICE_IMPL_H_
