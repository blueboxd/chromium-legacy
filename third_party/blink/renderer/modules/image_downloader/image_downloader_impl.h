// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_IMPL_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/image_downloader/image_downloader.mojom-blink.h"
#include "third_party/blink/renderer/modules/image_downloader/image_downloader_base.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class KURL;
class LocalFrame;

class ImageDownloaderImpl final
    : public GarbageCollectedFinalized<ImageDownloaderImpl>,
      public Supplement<LocalFrame>,
      public ImageDownloaderBase,
      public mojom::blink::ImageDownloader {
  USING_PRE_FINALIZER(ImageDownloaderImpl, Dispose);
  USING_GARBAGE_COLLECTED_MIXIN(ImageDownloaderImpl);

 public:
  static const char kSupplementName[];

  explicit ImageDownloaderImpl(LocalFrame&);
  ~ImageDownloaderImpl() override;

  static ImageDownloaderImpl* From(LocalFrame&);

  static void ProvideTo(LocalFrame&);

  void Trace(Visitor*) override;

 private:
  // Override ImageDownloaderBase::ContextDestroyed().
  void ContextDestroyed(ExecutionContext*) override;

  // ImageDownloader implementation.
  void DownloadImage(const KURL& url,
                     bool is_favicon,
                     uint32_t max_bitmap_size,
                     bool bypass_cache,
                     DownloadImageCallback callback) override;

  // Called when downloading finishes. All frames in |images| whose size <=
  // |max_image_size| will be returned through |callback|. If all of the frames
  // are larger than |max_image_size|, the smallest frame is resized to
  // |max_image_size| and is the only result. |max_image_size| == 0 is
  // interpreted as no max image size.
  void DidDownloadImage(uint32_t max_bitmap_size,
                        DownloadImageCallback callback,
                        int32_t http_status_code,
                        const WTF::Vector<SkBitmap>& images);

  void CreateMojoService(mojom::blink::ImageDownloaderRequest request);

  // USING_PRE_FINALIZER interface.
  // Called before the object gets garbage collected.
  void Dispose();

  mojo::Binding<mojom::blink::ImageDownloader> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageDownloaderImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_IMPL_H_
