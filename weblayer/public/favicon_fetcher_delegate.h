// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_FAVICON_FETCHER_DELEGATE_H_
#define WEBLAYER_PUBLIC_FAVICON_FETCHER_DELEGATE_H_

#include "base/observer_list.h"

namespace gfx {
class Image;
}

namespace weblayer {

// Notified of interesting events related to FaviconFetcher.
class FaviconFetcherDelegate : public base::CheckedObserver {
 public:
  // Called when the favicon of the current navigation has changed. This may be
  // called multiple times for the same navigation. This is *not* immediately
  // called with an empty image when a navigation starts. It is assumed
  // consuming code asks for the favicon when the navigation changes.
  virtual void OnFaviconChanged(const gfx::Image& image) = 0;

 protected:
  ~FaviconFetcherDelegate() override = default;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_FAVICON_FETCHER_DELEGATE_H_
