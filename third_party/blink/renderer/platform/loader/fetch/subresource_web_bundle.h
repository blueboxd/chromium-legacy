// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SUBRESOURCE_WEB_BUNDLE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SUBRESOURCE_WEB_BUNDLE_H_

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class KURL;

// SubresourceWebBundle is attached to ResourceFetcher and used to intercept
// subresource requests for a certain set of URLs and serve responses from a
// WebBundle. This is used for Subresource loading with Web Bundles
// (https://github.com/WICG/webpackage/blob/master/explainers/subresource-loading.md).
class PLATFORM_EXPORT SubresourceWebBundle : public GarbageCollectedMixin {
 public:
  void Trace(Visitor* visitor) const override {}
  virtual bool CanHandleRequest(const KURL& url) const = 0;
  virtual mojo::PendingRemote<network::mojom::blink::URLLoaderFactory>
  GetURLLoaderFactory() = 0;
  virtual String GetCacheIdentifier() const = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_SUBRESOURCE_WEB_BUNDLE_H_
