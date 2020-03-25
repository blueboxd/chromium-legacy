// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_INFO_FETCHER_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_INFO_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/upboarding/query_tiles/internal/query_tile_types.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace upboarding {

class TileInfoFetcher {
 public:
  // Called after the fetch task is done, |status| and serialized response
  // |data| will be returned. Invoked with |nullptr| if status is not success.
  using FinishedCallback = base::OnceCallback<void(
      TileInfoRequestStatus status,
      const std::unique_ptr<std::string> response_body)>;

  // Method to create a fetcher and start the fetch task immediately.
  static std::unique_ptr<TileInfoFetcher> CreateAndFetchForTileInfo(
      const GURL& url,
      const std::string& locale,
      const std::string& accept_languages,
      const std::string& api_key,
      const net::NetworkTrafficAnnotationTag& traffic_annotation,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      FinishedCallback callback);

  virtual ~TileInfoFetcher();

  TileInfoFetcher(const TileInfoFetcher& other) = delete;
  TileInfoFetcher& operator=(const TileInfoFetcher& other) = delete;

 protected:
  TileInfoFetcher();
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_TILE_INFO_FETCHER_H_
