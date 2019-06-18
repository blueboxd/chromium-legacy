// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/fetchers/multi_resolution_image_resource_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "content/child/image_decoder.h"
#include "content/public/renderer/associated_resource_fetcher.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

using blink::WebLocalFrame;
using blink::WebAssociatedURLLoaderOptions;
using blink::WebURLRequest;
using blink::WebURLResponse;

namespace content {

MultiResolutionImageResourceFetcher::MultiResolutionImageResourceFetcher(
    const GURL& image_url,
    WebLocalFrame* frame,
    int id,
    blink::mojom::RequestContextType request_context,
    blink::mojom::FetchCacheMode cache_mode,
    Callback callback)
    : callback_(std::move(callback)),
      id_(id),
      http_status_code_(0),
      image_url_(image_url) {
  fetcher_.reset(AssociatedResourceFetcher::Create(image_url));

  WebAssociatedURLLoaderOptions options;
  fetcher_->SetLoaderOptions(options);

  if (request_context == blink::mojom::RequestContextType::FAVICON) {
    // To prevent cache tainting, the cross-origin favicon requests have to
    // by-pass the service workers. This should ideally not happen. But Chrome’s
    // ThumbnailDatabase is using the icon URL as a key of the "favicons" table.
    // So if we don't set the skip flag here, malicious service workers can
    // override the favicon image of any origins.
    if (!frame->GetDocument().GetSecurityOrigin().CanAccess(
            blink::WebSecurityOrigin::Create(image_url_))) {
      fetcher_->SetSkipServiceWorker(true);
    }
  }

  fetcher_->SetCacheMode(cache_mode);

  fetcher_->Start(
      frame, request_context, network::mojom::RequestMode::kNoCors,
      network::mojom::CredentialsMode::kInclude,
      base::BindOnce(&MultiResolutionImageResourceFetcher::OnURLFetchComplete,
                     base::Unretained(this)));
}

MultiResolutionImageResourceFetcher::~MultiResolutionImageResourceFetcher() {
}

void MultiResolutionImageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  std::vector<SkBitmap> bitmaps;
  if (!response.IsNull()) {
    http_status_code_ = response.HttpStatusCode();
    GURL url(response.CurrentRequestUrl());
    if (http_status_code_ == 200 || url.SchemeIsFile()) {
      // Request succeeded, try to convert it to an image.
      bitmaps = ImageDecoder::DecodeAll(
          reinterpret_cast<const unsigned char*>(data.data()), data.size());
    }
  } // else case:
    // If we get here, it means no image from server or couldn't decode the
    // response as an image. The delegate will see an empty vector.

  std::move(callback_).Run(this, bitmaps);
}

void MultiResolutionImageResourceFetcher::OnRenderFrameDestruct() {
  std::move(callback_).Run(this, std::vector<SkBitmap>());
}

}  // namespace content
