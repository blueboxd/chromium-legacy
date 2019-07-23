// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/image_downloader/image_downloader_base.h"

#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/platform/web_data.h"
#include "third_party/blink/public/platform/web_image.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/modules/image_downloader/fetcher/multi_resolution_image_resource_fetcher.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/network/network_utils.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/favicon_size.h"

namespace {

// Decodes a data: URL image or returns an empty image in case of failure.
SkBitmap ImageFromDataUrl(const blink::KURL& url) {
  std::string data;
  if (blink::network_utils::IsDataURLMimeTypeSupported(url, &data) &&
      !data.empty()) {
    gfx::Size desired_icon_size =
        gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize);
    const unsigned char* src_data =
        reinterpret_cast<const unsigned char*>(data.data());
    return blink::WebImage::FromData(
        blink::WebData(reinterpret_cast<const char*>(src_data), data.size()),
        blink::WebSize(desired_icon_size));
  }
  return SkBitmap();
}

}  // namespace

namespace blink {

ImageDownloaderBase::ImageDownloaderBase(ExecutionContext* context,
                                         LocalFrame& frame)
    : ContextLifecycleObserver(context), frame_(frame) {}

ImageDownloaderBase::~ImageDownloaderBase() {}

void ImageDownloaderBase::DownloadImage(const KURL& image_url,
                                        bool is_favicon,
                                        bool bypass_cache,
                                        DownloadCallback callback) {
  if (!image_url.ProtocolIsData()) {
    FetchImage(image_url, is_favicon, bypass_cache, std::move(callback));
    // Will complete asynchronously via ImageDownloaderBase::DidFetchImage.
    return;
  }

  WTF::Vector<SkBitmap> result_images;
  SkBitmap data_image = ImageFromDataUrl(image_url);

  // Drop null or empty SkBitmap.
  if (!data_image.drawsNothing())
    result_images.push_back(data_image);

  std::move(callback).Run(0, result_images);
}

void ImageDownloaderBase::FetchImage(const KURL& image_url,
                                     bool is_favicon,
                                     bool bypass_cache,
                                     DownloadCallback callback) {
  // Create an image resource fetcher and assign it with a call back object.
  image_fetchers_.push_back(
      std::make_unique<MultiResolutionImageResourceFetcher>(
          image_url, frame_, 0,
          is_favicon ? blink::mojom::RequestContextType::FAVICON
                     : blink::mojom::RequestContextType::IMAGE,
          bypass_cache ? blink::mojom::FetchCacheMode::kBypassCache
                       : blink::mojom::FetchCacheMode::kDefault,
          WTF::Bind(&ImageDownloaderBase::DidFetchImage, WrapPersistent(this),
                    std::move(callback))));
}

void ImageDownloaderBase::DidFetchImage(
    DownloadCallback callback,
    MultiResolutionImageResourceFetcher* fetcher,
    const WTF::Vector<SkBitmap>& images) {
  int32_t http_status_code = fetcher->http_status_code();

  // Remove the image fetcher from our pending list. We're in the callback from
  // MultiResolutionImageResourceFetcher, best to delay deletion.
  for (auto* it = image_fetchers_.begin(); it != image_fetchers_.end(); ++it) {
    MultiResolutionImageResourceFetcher* image_fetcher = it->get();
    DCHECK(image_fetcher);
    if (image_fetcher == fetcher) {
      it = image_fetchers_.erase(it);
      break;
    }
  }

  // |this| may be destructed after callback is run.
  std::move(callback).Run(http_status_code, images);
}

void ImageDownloaderBase::Trace(blink::Visitor* visitor) {
  visitor->Trace(frame_);
  ContextLifecycleObserver::Trace(visitor);
}

void ImageDownloaderBase::ContextDestroyed(ExecutionContext*) {
  for (const auto& fetchers : image_fetchers_) {
    // Will run callbacks with an empty image vector.
    fetchers->OnRenderFrameDestruct();
  }
  image_fetchers_.clear();
}

}  // namespace blink
