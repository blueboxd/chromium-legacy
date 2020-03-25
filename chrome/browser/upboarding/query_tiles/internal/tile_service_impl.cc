// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/tile_service_impl.h"

#include "ui/gfx/image/image.h"

namespace upboarding {

TileServiceImpl::TileServiceImpl(image_fetcher::ImageFetcher* image_fetcher) {}

TileServiceImpl::~TileServiceImpl() = default;

void TileServiceImpl::GetQueryTiles(GetTilesCallback callback) {
  NOTIMPLEMENTED();
}

void TileServiceImpl::GetVisuals(const std::string& tile_id,
                                 VisualsCallback callback) {
  NOTIMPLEMENTED();
}

}  // namespace upboarding
