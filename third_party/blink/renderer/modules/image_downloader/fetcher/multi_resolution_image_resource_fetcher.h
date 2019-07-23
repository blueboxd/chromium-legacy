// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_FETCHER_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_FETCHER_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"

class SkBitmap;

namespace blink {
class LocalFrame;
class WebURLResponse;
class KURL;

class AssociatedResourceFetcher;

// A resource fetcher that returns all (differently-sized) frames in
// an image. Useful for favicons.
class MultiResolutionImageResourceFetcher {
  USING_FAST_MALLOC(MultiResolutionImageResourceFetcher);

 public:
  using Callback = base::OnceCallback<void(MultiResolutionImageResourceFetcher*,
                                           const WTF::Vector<SkBitmap>&)>;

  MultiResolutionImageResourceFetcher(
      const KURL& image_url,
      LocalFrame* frame,
      int id,
      mojom::blink::RequestContextType request_context,
      mojom::blink::FetchCacheMode cache_mode,
      Callback callback);

  virtual ~MultiResolutionImageResourceFetcher();

  // URL of the image we're downloading.
  const KURL& image_url() const { return image_url_; }

  // Unique identifier for the request.
  int id() const { return id_; }

  // HTTP status code upon fetch completion.
  int http_status_code() const { return http_status_code_; }

  // Called when associated LocalFrame is destructed.
  void OnRenderFrameDestruct();

 private:
  // ResourceFetcher::Callback. Decodes the image and invokes callback_.
  void OnURLFetchComplete(const WebURLResponse& response,
                          const std::string& data);

  Callback callback_;

  // Unique identifier for the request.
  const int id_;

  // HTTP status code upon fetch completion.
  int http_status_code_;

  // URL of the image.
  const KURL image_url_;

  // Does the actual download.
  std::unique_ptr<AssociatedResourceFetcher> fetcher_;

  DISALLOW_COPY_AND_ASSIGN(MultiResolutionImageResourceFetcher);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_FETCHER_MULTI_RESOLUTION_IMAGE_RESOURCE_FETCHER_H_
