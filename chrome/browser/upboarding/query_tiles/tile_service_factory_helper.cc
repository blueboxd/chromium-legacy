// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/tile_service_factory_helper.h"

#include "chrome/browser/upboarding/query_tiles/internal/cached_image_loader.h"
#include "chrome/browser/upboarding/query_tiles/internal/tile_service_impl.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/keyed_service/core/keyed_service.h"

namespace upboarding {

std::unique_ptr<TileService> CreateTileService(
    image_fetcher::ImageFetcherService* image_fetcher_service) {
  auto* cached_image_fetcher = image_fetcher_service->GetImageFetcher(
      image_fetcher::ImageFetcherConfig::kDiskCacheOnly);
  auto* reduced_mode_image_fetcher = image_fetcher_service->GetImageFetcher(
      image_fetcher::ImageFetcherConfig::kReducedMode);
  auto image_loader = std::make_unique<CachedImageLoader>(
      cached_image_fetcher, reduced_mode_image_fetcher);
  return std::make_unique<TileServiceImpl>(std::move(image_loader));
}

}  // namespace upboarding
