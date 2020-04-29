// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IMAGE_DOWNLOADER_H_
#define ASH_PUBLIC_CPP_IMAGE_DOWNLOADER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback_forward.h"

class GURL;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace net {
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash {

// Interface for a class which is responsible for downloading images in ash.
class ASH_PUBLIC_EXPORT ImageDownloader {
 public:
  static ImageDownloader* Get();

  using DownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

  // Downloads the image found at |url| for the primary profile. On completion,
  // |callback| is run with the downloaded |image|. In the event that the
  // download attempt fails, a nullptr image will be returned.
  virtual void Download(const GURL& url,
                        const net::NetworkTrafficAnnotationTag& annotation_tag,
                        DownloadCallback callback) = 0;

 protected:
  ImageDownloader();
  ImageDownloader(const ImageDownloader&) = delete;
  ImageDownloader& operator=(const ImageDownloader&) = delete;
  virtual ~ImageDownloader();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IMAGE_DOWNLOADER_H_
