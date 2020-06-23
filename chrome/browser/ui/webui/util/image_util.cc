// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/util/image_util.h"

#include "base/base64.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/image/image_skia.h"

namespace {
// Writes bytes to a std::vector that can be fetched. This is used to record the
// output of skia image encoding.
class BufferWStream : public SkWStream {
 public:
  BufferWStream() = default;
  ~BufferWStream() override = default;

  // Returns the output buffer by moving.
  std::vector<unsigned char> GetBuffer() { return std::move(result_); }

  // SkWStream:
  bool write(const void* buffer, size_t size) override {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(buffer);
    result_.insert(result_.end(), bytes, bytes + size);
    return true;
  }

  size_t bytesWritten() const override { return result_.size(); }

 private:
  std::vector<unsigned char> result_;
};
}  // namespace

namespace webui {

std::string MakeDataURIForImage(base::span<const uint8_t> image_data,
                                base::StringPiece mime_subtype) {
  std::string result = "data:image/";
  result.append(mime_subtype.begin(), mime_subtype.end());
  result += ";base64,";
  result += base::Base64Encode(image_data);
  return result;
}

std::string EncodePNGAndMakeDataURI(gfx::ImageSkia image, float scale_factor) {
  const SkBitmap& bitmap = image.GetRepresentation(scale_factor).GetBitmap();
  BufferWStream stream;
  const bool encoding_succeeded =
      SkEncodeImage(&stream, bitmap, SkEncodedImageFormat::kPNG, 100);
  DCHECK(encoding_succeeded);
  return MakeDataURIForImage(
      base::as_bytes(base::make_span(stream.GetBuffer())), "png");
}

}  // namespace webui
