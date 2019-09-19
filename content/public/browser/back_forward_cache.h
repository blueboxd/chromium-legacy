// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_BACK_FORWARD_CACHE_H_
#define CONTENT_PUBLIC_BROWSER_BACK_FORWARD_CACHE_H_

#include <string_view>

#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"

namespace content {

// Public API for the BackForwardCache.
//
// After the user navigates away from a document, the old one might go into the
// frozen state and will be kept in the cache. It can potentially be reused
// at a later time if the user navigates back.
//
// Not all documents can or will be cached. You should not assume a document
// will be cached.
//
// WARNING: This code is still experimental and might completely go away.
// Please get in touch with bfcache-dev@chromium.org if you intend to use it.
//
// All methods of this class should be called from the UI thread.
class CONTENT_EXPORT BackForwardCache {
 public:
  // Prevents the frame for the given |id| from entering the BackForwardCache. A
  // document can only enter the BackForwardCache if the root frame and all its
  // children can. This action can not be undone. Any document that is assigned
  // to this same frame in the future will not be cached either. In practice
  // this is not a big deal as only navigations that use a new frame can be
  // cached.
  //
  // This might be needed for example by components that listen to events via a
  // WebContentsObserver and keep some sort of per frame state, as this state
  // might be lost and not be recreated when navigating back.
  //
  // If the page is already in the cache an eviction is triggered.
  //
  // |id|: If no RenderFrameHost can be found for the given id nothing happens.
  // |reason|: Free form string to be used in logging and metrics.
  virtual void DisableForRenderFrameHost(GlobalFrameRoutingId id,
                                         std::string_view reason) = 0;

 protected:
  BackForwardCache() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_BACK_FORWARD_CACHE_H_
