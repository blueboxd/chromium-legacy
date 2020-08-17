// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_V2_IMAGE_FETCHER_H_
#define COMPONENTS_FEED_CORE_V2_IMAGE_FETCHER_H_

#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
class SimpleURLLoader;
}  // namespace network

namespace feed {

struct NetworkResponse;

// Fetcher object to retrieve an image resource from a URL.
class ImageFetcher {
 public:
  using ImageCallback = base::OnceCallback<void(NetworkResponse)>;
  explicit ImageFetcher(
      scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory);
  virtual ~ImageFetcher();
  ImageFetcher(const ImageFetcher&) = delete;
  ImageFetcher& operator=(const ImageFetcher&) = delete;

  virtual void Fetch(const GURL& url, ImageCallback callback);

 private:
  // Called when fetch request completes.
  void OnFetchComplete(std::unique_ptr<network::SimpleURLLoader> simple_loader,
                       ImageCallback callback,
                       std::unique_ptr<std::string> response_data);

  const scoped_refptr<::network::SharedURLLoaderFactory> url_loader_factory_;
  base::WeakPtrFactory<ImageFetcher> weak_factory_{this};
};

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_V2_IMAGE_FETCHER_H_
