// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_BASE_H_

#include <stdint.h>

#include "base/callback.h"
#include "third_party/blink/renderer/core/execution_context/context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

class KURL;
class MultiResolutionImageResourceFetcher;
class LocalFrame;

class ImageDownloaderBase : public ContextLifecycleObserver {
 public:
  ImageDownloaderBase(ExecutionContext*, LocalFrame&);
  ~ImageDownloaderBase();

  using DownloadCallback =
      base::OnceCallback<void(int32_t, const WTF::Vector<SkBitmap>&)>;

  // Request to asynchronously download an image. When done, |callback| will be
  // called.
  void DownloadImage(const KURL& url,
                     bool is_favicon,
                     bool bypass_cache,
                     DownloadCallback callback);

  void Trace(blink::Visitor*) override;

 protected:
  // ContextLifecycleObserver:
  void ContextDestroyed(ExecutionContext*) override;

 private:
  // Requests to fetch an image. When done, the image downloader is notified by
  // way of DidFetchImage. If the image is a favicon, cookies will not be sent
  // nor accepted during download. If the image has multiple frames, all frames
  // are returned.
  void FetchImage(const KURL& image_url,
                  bool is_favicon,
                  bool bypass_cache,
                  DownloadCallback callback);

  // This callback is triggered when FetchImage completes, either
  // successfully or with a failure. See FetchImage for more
  // details.
  void DidFetchImage(DownloadCallback callback,
                     MultiResolutionImageResourceFetcher* fetcher,
                     const WTF::Vector<SkBitmap>& images);

  // ImageResourceFetchers schedule via FetchImage.
  HeapVector<Member<MultiResolutionImageResourceFetcher>> image_fetchers_;

  Member<LocalFrame> frame_;

  DISALLOW_COPY_AND_ASSIGN(ImageDownloaderBase);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IMAGE_DOWNLOADER_IMAGE_DOWNLOADER_BASE_H_
