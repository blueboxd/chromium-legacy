// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/tile_service_factory_helper.h"

#include "chrome/browser/upboarding/query_tiles/internal/tile_service_impl.h"
#include "components/keyed_service/core/keyed_service.h"

namespace upboarding {

std::unique_ptr<TileService> CreateTileService(
    image_fetcher::ImageFetcher* image_fetcher) {
  return std::make_unique<TileServiceImpl>(image_fetcher);
}

}  // namespace upboarding
