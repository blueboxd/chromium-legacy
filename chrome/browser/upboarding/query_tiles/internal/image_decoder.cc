// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/image_decoder.h"

#include <utility>

#include "services/data_decoder/public/cpp/decode_image.h"
#include "ui/gfx/geometry/size.h"

namespace upboarding {
namespace {

// The maximum size of data to be decoded.
constexpr size_t kMaximumEncodedDataSize = 10 * 1024 * 1024 /*10MB*/;

// Decodes an image in a utility process. Destroy the object will release the
// IPC connection.
class SafeImageDecoder : public ImageDecoder {
 public:
  SafeImageDecoder() = default;
  SafeImageDecoder(const SafeImageDecoder&) = delete;
  SafeImageDecoder& operator=(const SafeImageDecoder&) = delete;
  ~SafeImageDecoder() override = default;

 private:
  // ImageDecoder implementation.
  void Decode(EncodedData data,
              const gfx::Size& output_size,
              DecodeCallback decode_callback) override {
    DCHECK(decode_callback);
    if (data.empty()) {
      std::move(decode_callback).Run(SkBitmap());
      return;
    }

    // Each decoding operation happens in its own process.
    // TODO(xingliu): Consider to use a shared utility process.
    data_decoder::DecodeImageIsolated(
        std::move(data), data_decoder::mojom::ImageCodec::DEFAULT,
        /*shrink_to_fit=*/true, kMaximumEncodedDataSize, output_size,
        std::move(decode_callback));
  }
};

}  // namespace

// static
std::unique_ptr<ImageDecoder> ImageDecoder::Create() {
  return std::make_unique<SafeImageDecoder>();
}

ImageDecoder::ImageDecoder() = default;

ImageDecoder::~ImageDecoder() = default;

}  // namespace upboarding
