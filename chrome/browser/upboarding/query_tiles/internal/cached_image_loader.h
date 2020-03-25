// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_CACHED_IMAGE_LOADER_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_CACHED_IMAGE_LOADER_H_

#include <memory>

#include "chrome/browser/upboarding/query_tiles/internal/image_loader.h"
#include "components/image_fetcher/core/image_fetcher.h"

namespace image_fetcher {
class ImageFetcher;
}  // namespace image_fetcher

namespace upboarding {

// Loads image with image fetcher service, which provides a disk cache to reduce
// network data consumption.
class CachedImageLoader : public ImageLoader {
 public:
  explicit CachedImageLoader(image_fetcher::ImageFetcher* image_fetcher);
  ~CachedImageLoader() override;

 private:
  // ImageLoader implementation.
  void FetchImage(const GURL& url, BitmapCallback callback) override;

  // Owned by ImageFetcherService. Outlives TileService, the owner of this
  // class.
  image_fetcher::ImageFetcher* image_fetcher_;
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_CACHED_IMAGE_LOADER_H_
