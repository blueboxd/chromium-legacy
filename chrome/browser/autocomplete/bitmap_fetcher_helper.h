// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOCOMPLETE_BITMAP_FETCHER_HELPER_H_
#define CHROME_BROWSER_AUTOCOMPLETE_BITMAP_FETCHER_HELPER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_service.h"

namespace content {
class BrowserContext;
}  // namespace content

class GURL;
class SkBitmap;

class BitmapFetcherHelper {
 public:
  using BitmapFetchedCallback =
      base::RepeatingCallback<void(const SkBitmap& bitmap)>;

  explicit BitmapFetcherHelper(content::BrowserContext* context);
  virtual ~BitmapFetcherHelper();

  // Requests the image at |image_url| and returns the request ID. |callback|
  // will be called with either a cached or a downloaded image if the request is
  // successful or with an empty one to signal failure.
  BitmapFetcherService::RequestId RequestImage(const GURL& image_url,
                                               BitmapFetchedCallback callback);

  // Cancels |request| if it is still in-flight.
  void CancelRequest(BitmapFetcherService::RequestId request);

  // Start fetching the image at |image_url|.
  void PrefetchImage(const GURL& image_url);

 private:
  BitmapFetcherService* const bitmap_fetcher_service_;

  DISALLOW_COPY_AND_ASSIGN(BitmapFetcherHelper);
};

#endif  // CHROME_BROWSER_AUTOCOMPLETE_BITMAP_FETCHER_HELPER_H_
