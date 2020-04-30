// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/query_tiles/internal/tile_service_impl.h"

#include <string>
#include <utility>

#include "ui/gfx/image/image.h"

namespace upboarding {

TileServiceImpl::TileServiceImpl(std::unique_ptr<ImageLoader> image_loader,
                                 std::unique_ptr<TileManager> tile_manager,
                                 std::unique_ptr<TileConfig> config)
    : image_loader_(std::move(image_loader)),
      tile_manager_(std::move(tile_manager)),
      config_(std::move(config)) {}

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

}  // namespace upboarding
