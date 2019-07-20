// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/image_downloader/image_downloader_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "skia/ext/image_operations.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "ui/gfx/geometry/size.h"

namespace {

//  Proportionally resizes the |image| to fit in a box of size
// |max_image_size|.
SkBitmap ResizeImage(const SkBitmap& image, uint32_t max_image_size) {
  if (max_image_size == 0)
    return image;
  uint32_t max_dimension = std::max(image.width(), image.height());
  if (max_dimension <= max_image_size)
    return image;
  // Proportionally resize the minimal image to fit in a box of size
  // max_image_size.
  return skia::ImageOperations::Resize(
      image, skia::ImageOperations::RESIZE_BEST,
      static_cast<uint32_t>(image.width()) * max_image_size / max_dimension,
      static_cast<uint32_t>(image.height()) * max_image_size / max_dimension);
}

// Filters the array of bitmaps, removing all images that do not fit in a box of
// size |max_image_size|. Returns the result if it is not empty. Otherwise,
// find the smallest image in the array and resize it proportionally to fit
// in a box of size |max_image_size|.
// Sets |original_image_sizes| to the sizes of |images| before resizing.
void FilterAndResizeImagesForMaximalSize(
    const WTF::Vector<SkBitmap>& unfiltered,
    uint32_t max_image_size,
    WTF::Vector<SkBitmap>* images,
    WTF::Vector<blink::WebSize>* original_image_sizes) {
  images->clear();
  original_image_sizes->clear();

  if (unfiltered.IsEmpty())
    return;

  if (max_image_size == 0)
    max_image_size = std::numeric_limits<uint32_t>::max();

  const SkBitmap* min_image = nullptr;
  uint32_t min_image_size = std::numeric_limits<uint32_t>::max();
  // Filter the images by |max_image_size|, and also identify the smallest image
  // in case all the images are bigger than |max_image_size|.
  for (auto* it = unfiltered.begin(); it != unfiltered.end(); ++it) {
    const SkBitmap& image = *it;
    uint32_t current_size = std::max(it->width(), it->height());
    if (current_size < min_image_size) {
      min_image = &image;
      min_image_size = current_size;
    }
    if (static_cast<uint32_t>(image.width()) <= max_image_size &&
        static_cast<uint32_t>(image.height()) <= max_image_size) {
      images->push_back(image);
      original_image_sizes->push_back(gfx::Size(image.width(), image.height()));
    }
  }
  DCHECK(min_image);
  if (images->size())
    return;
  // Proportionally resize the minimal image to fit in a box of size
  // |max_image_size|.
  SkBitmap resized = ResizeImage(*min_image, max_image_size);
  // Drop null or empty SkBitmap.
  if (resized.drawsNothing())
    return;
  images->push_back(resized);
  original_image_sizes->push_back(
      gfx::Size(min_image->width(), min_image->height()));
}

}  // namespace

namespace blink {

// static
const char ImageDownloaderImpl::kSupplementName[] = "ImageDownloader";

// static
ImageDownloaderImpl* ImageDownloaderImpl::From(LocalFrame& frame) {
  return Supplement<LocalFrame>::From<ImageDownloaderImpl>(frame);
}

// static
void ImageDownloaderImpl::ProvideTo(LocalFrame& frame) {
  if (ImageDownloaderImpl::From(frame))
    return;
  Supplement<LocalFrame>::ProvideTo(
      frame, MakeGarbageCollected<ImageDownloaderImpl>(frame));
}

ImageDownloaderImpl::ImageDownloaderImpl(LocalFrame& frame)
    : Supplement<LocalFrame>(frame),
      ImageDownloaderBase(frame.GetDocument()->GetExecutionContext(), frame) {
  frame.GetInterfaceRegistry()->AddInterface(WTF::BindRepeating(
      &ImageDownloaderImpl::CreateMojoService, WrapWeakPersistent(this)));
}

ImageDownloaderImpl::~ImageDownloaderImpl() {}

void ImageDownloaderImpl::CreateMojoService(
    mojom::blink::ImageDownloaderRequest request) {
  binding_.Bind(std::move(request));
  binding_.set_connection_error_handler(
      WTF::Bind(&ImageDownloaderImpl::Dispose, WrapWeakPersistent(this)));
}

// ImageDownloader methods:
void ImageDownloaderImpl::DownloadImage(const KURL& image_url,
                                        bool is_favicon,
                                        uint32_t max_bitmap_size,
                                        bool bypass_cache,
                                        DownloadImageCallback callback) {
  WTF::Vector<SkBitmap> result_images;
  WTF::Vector<gfx::Size> result_original_image_sizes;

  ImageDownloaderBase::DownloadImage(
      image_url, is_favicon, bypass_cache,
      WTF::Bind(&ImageDownloaderImpl::DidDownloadImage, WrapPersistent(this),
                max_bitmap_size, std::move(callback)));
}

void ImageDownloaderImpl::DidDownloadImage(
    uint32_t max_image_size,
    DownloadImageCallback callback,
    int32_t http_status_code,
    const WTF::Vector<SkBitmap>& images) {
  WTF::Vector<SkBitmap> result_images;
  WTF::Vector<WebSize> result_original_image_sizes;
  FilterAndResizeImagesForMaximalSize(images, max_image_size, &result_images,
                                      &result_original_image_sizes);

  std::move(callback).Run(http_status_code, result_images,
                          result_original_image_sizes);
}

void ImageDownloaderImpl::ContextDestroyed(ExecutionContext* context) {
  ImageDownloaderBase::ContextDestroyed(context);
  Dispose();
}

void ImageDownloaderImpl::Dispose() {
  binding_.Close();
}

void ImageDownloaderImpl::Trace(Visitor* visitor) {
  Supplement<LocalFrame>::Trace(visitor);
  ImageDownloaderBase::Trace(visitor);
}

}  // namespace blink
