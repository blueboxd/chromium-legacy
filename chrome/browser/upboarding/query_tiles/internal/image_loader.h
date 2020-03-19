// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_LOADER_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_LOADER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "url/gurl.h"

class SkBitmap;

namespace upboarding {

// Loads image for query tiles. The image will be fetched from a URL and cached
// on disk.
class ImageLoader {
 public:
  using SuccessCallback = base::OnceCallback<bool>;
  using BitmapCallback = base::OnceCallback<std::unique_ptr<SkBitmap>>;
  using Id = std::string;

  ImageLoader();
  virtual ~ImageLoader();

  // Updates the image cache for a specific tile. If the URL is changed, we will
  // immediately fetch the image, then invoke the callback.
  virtual void Update(const Id& id,
                      const GURL& url,
                      SuccessCallback callback) = 0;
  // Deletes an image cache for a specific tile.
  virtual void Delete(const Id& id, SuccessCallback callback) = 0;

  // Gets the bitmap for a specific tile. Callback will be invoked after
  // reading the data from disk or the fetch is done.
  void GetBitmap(const Id& id, BitmapCallback callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageLoader);
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_LOADER_H_
