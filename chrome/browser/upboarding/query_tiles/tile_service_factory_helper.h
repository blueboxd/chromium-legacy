// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_TILE_SERVICE_FACTORY_HELPER_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_TILE_SERVICE_FACTORY_HELPER_H_

#include <memory>

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace upboarding {

class TileService;

std::unique_ptr<TileService> CreateTileService(
    image_fetcher::ImageFetcher* image_fetcher);

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_TILE_SERVICE_FACTORY_HELPER_H_
