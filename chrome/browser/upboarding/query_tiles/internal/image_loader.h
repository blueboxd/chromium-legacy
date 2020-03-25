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

// Loads image for query tiles.
class ImageLoader {
 public:
  using BitmapCallback = base::OnceCallback<void(SkBitmap bitmap)>;

  ImageLoader() = default;
  virtual ~ImageLoader() = default;
  ImageLoader(const ImageLoader&) = delete;
  ImageLoader& operator=(const ImageLoader&) = delete;

  // Fetches the bitmap of an image based on its URL. Callback will be invoked
  // with the bitmap of the image, or an empty bitmap on failure.
  virtual void FetchImage(const GURL& url, BitmapCallback callback) = 0;
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_LOADER_H_
