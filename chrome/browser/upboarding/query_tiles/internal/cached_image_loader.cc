// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/cached_image_loader.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image.h"

namespace upboarding {
namespace {

// A string used to log UMA for query tiles in image fetcher service.
constexpr char kImageFetcherUmaClientName[] = "QueryTiles";

constexpr net::NetworkTrafficAnnotationTag kQueryTilesTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("query_tiles_image_loader", R"(
      semantics {
        sender: "Query Tiles Image Loader"
        description:
          "Fetches image for query tiles on Android NTP. Images are hosted on"
          " Google static content server, the data source may come from third"
          " parties."
        trigger:
          "When the user opens NTP to view the query tiles on Android, and"
          " the image cache doesn't have a fresh copy on disk."
        data: "URL of the image to be fetched."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting: "Disabled when the user uses search engines other than Google."
        chrome_policy {
            DefaultSearchProviderEnabled {
              policy_options {mode: MANDATORY}
              DefaultSearchProviderEnabled: false
            }
        }
      })");

void OnImageFetched(ImageLoader::BitmapCallback callback,
                    const gfx::Image& image,
                    const image_fetcher::RequestMetadata&) {
  DCHECK(callback);
  std::move(callback).Run(image.AsBitmap());
}

}  // namespace

CachedImageLoader::CachedImageLoader(image_fetcher::ImageFetcher* image_fetcher)
    : image_fetcher_(image_fetcher) {
  DCHECK(image_fetcher_);
}

CachedImageLoader::~CachedImageLoader() = default;

void CachedImageLoader::FetchImage(const GURL& url, BitmapCallback callback) {
  // Fetch and decode the image from network or disk cache.
  // TODO(xingliu): Add custom expiration to ImageFetcherParams.
  image_fetcher::ImageFetcherParams params(kQueryTilesTrafficAnnotation,
                                           kImageFetcherUmaClientName);
  image_fetcher_->FetchImage(
      url, base::BindOnce(&OnImageFetched, std::move(callback)), params);
}

}  // namespace upboarding
