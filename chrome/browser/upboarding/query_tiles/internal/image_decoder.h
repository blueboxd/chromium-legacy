// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_DECODER_H_
#define CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_DECODER_H_

#include <memory>
#include <vector>

#include "base/callback.h"

namespace gfx {
class Size;
}  // namespace gfx

class SkBitmap;

namespace upboarding {

// Decodes an image into bitmap, to be consumed by UI.
class ImageDecoder {
 public:
  using DecodeCallback = base::OnceCallback<void(const SkBitmap&)>;
  using EncodedData = std::vector<uint8_t>;

  static std::unique_ptr<ImageDecoder> Create();

  ImageDecoder(const ImageDecoder&) = delete;
  ImageDecoder& operator=(const ImageDecoder&) = delete;
  virtual ~ImageDecoder();

  // Decodes the image. |data| should be moved into this function.
  virtual void Decode(EncodedData data,
                      const gfx::Size& output_size,
                      DecodeCallback decode_callback) = 0;

 protected:
  ImageDecoder();
};

}  // namespace upboarding

#endif  // CHROME_BROWSER_UPBOARDING_QUERY_TILES_INTERNAL_IMAGE_DECODER_H_
