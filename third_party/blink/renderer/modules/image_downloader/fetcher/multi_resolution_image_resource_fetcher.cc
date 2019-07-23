// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/image_downloader/fetcher/multi_resolution_image_resource_fetcher.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_image.h"
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/image_downloader/fetcher/associated_resource_fetcher.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

MultiResolutionImageResourceFetcher::MultiResolutionImageResourceFetcher(
    const KURL& image_url,
    LocalFrame* frame,
    int id,
    mojom::blink::RequestContextType request_context,
    mojom::blink::FetchCacheMode cache_mode,
    Callback callback)
    : callback_(std::move(callback)),
      id_(id),
      http_status_code_(0),
      image_url_(image_url),
      fetcher_(std::make_unique<AssociatedResourceFetcher>(image_url)) {
  WebAssociatedURLLoaderOptions options;
  fetcher_->SetLoaderOptions(options);

  if (request_context == mojom::blink::RequestContextType::FAVICON) {
    // To prevent cache tainting, the cross-origin favicon requests have to
    // by-pass the service workers. This should ideally not happen. But Chrome’s
    // ThumbnailDatabase is using the icon URL as a key of the "favicons" table.
    // So if we don't set the skip flag here, malicious service workers can
    // override the favicon image of any origins.
    if (!frame->GetDocument()->GetSecurityOrigin()->CanAccess(
            SecurityOrigin::Create(image_url_).get())) {
      fetcher_->SetSkipServiceWorker(true);
    }
  }

  fetcher_->SetCacheMode(cache_mode);

  fetcher_->Start(
      frame, request_context, network::mojom::RequestMode::kNoCors,
      network::mojom::CredentialsMode::kInclude,
      WTF::Bind(&MultiResolutionImageResourceFetcher::OnURLFetchComplete,
                WTF::Unretained(this)));
}

MultiResolutionImageResourceFetcher::~MultiResolutionImageResourceFetcher() {}

void MultiResolutionImageResourceFetcher::OnURLFetchComplete(
    const WebURLResponse& response,
    const std::string& data) {
  WTF::Vector<SkBitmap> bitmaps;
  if (!response.IsNull()) {
    http_status_code_ = response.HttpStatusCode();
    KURL url(response.CurrentRequestUrl());
    if (http_status_code_ == 200 || url.IsLocalFile()) {
      // Request succeeded, try to convert it to an image.
      const unsigned char* src_data =
          reinterpret_cast<const unsigned char*>(data.data());
      std::vector<SkBitmap> decoded_bitmaps =
          WebImage::FramesFromData(
              WebData(reinterpret_cast<const char*>(src_data), data.size()))
              .ReleaseVector();

      bitmaps.AppendRange(std::make_move_iterator(decoded_bitmaps.rbegin()),
                          std::make_move_iterator(decoded_bitmaps.rend()));
    }
  }  // else case:
     // If we get here, it means no image from server or couldn't decode the
     // response as an image. The delegate will see an empty vector.

  std::move(callback_).Run(this, bitmaps);
}

void MultiResolutionImageResourceFetcher::OnRenderFrameDestruct() {
  std::move(callback_).Run(this, WTF::Vector<SkBitmap>());
}

}  // namespace blink
