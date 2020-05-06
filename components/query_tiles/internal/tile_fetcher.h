// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_QUERY_TILES_INTERNAL_TILE_FETCHER_H_
#define COMPONENTS_QUERY_TILES_INTERNAL_TILE_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/query_tiles/internal/tile_types.h"
#include "net/base/backoff_entry.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace upboarding {

class TileFetcher {
 public:
  // Called after the fetch task is done, |status| and serialized response
  // |data| will be returned. Invoked with |nullptr| if status is not success.
  using FinishedCallback = base::OnceCallback<void(
      TileInfoRequestStatus status,
      const std::unique_ptr<std::string> response_body)>;

  // Method to create a TileFetcher.
  static std::unique_ptr<TileFetcher> Create(
      const GURL& url,
      const std::string& country_code,
      const std::string& accept_languages,
      const std::string& api_key,
      const std::string& experiment_tag,
      const scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);

  // Start the fetch to download tiles.
  virtual void StartFetchForTiles(FinishedCallback callback) = 0;

  virtual ~TileFetcher();

  TileFetcher(const TileFetcher& other) = delete;
  TileFetcher& operator=(const TileFetcher& other) = delete;

 protected:
  TileFetcher();
};

}  // namespace upboarding

#endif  // COMPONENTS_QUERY_TILES_INTERNAL_TILE_FETCHER_H_
