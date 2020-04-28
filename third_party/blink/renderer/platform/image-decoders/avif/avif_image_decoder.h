// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_AVIF_AVIF_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_AVIF_AVIF_IMAGE_DECODER_H_

#include <memory>

#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"

struct avifDecoder;
struct avifImage;

namespace blink {

class FastSharedBufferReader;

class PLATFORM_EXPORT AVIFImageDecoder final : public ImageDecoder {
 public:
  AVIFImageDecoder(AlphaOption,
                   HighBitDepthDecodingOption,
                   const ColorBehavior&,
                   size_t max_decoded_bytes);
  AVIFImageDecoder(const AVIFImageDecoder&) = delete;
  AVIFImageDecoder& operator=(const AVIFImageDecoder&) = delete;
  ~AVIFImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override { return "avif"; }
  bool ImageIsHighBitDepth() override;
  void OnSetData(SegmentReader* data) override;
  int RepetitionCount() const override;
  base::TimeDelta FrameDurationAtIndex(size_t) const override;

  // Returns true if the data in fast_reader begins with a valid FileTypeBox
  // (ftyp) that supports the brand 'avif' or 'avis'.
  static bool MatchesAVIFSignature(const FastSharedBufferReader& fast_reader);

 private:
  // ImageDecoder:
  void DecodeSize() override;
  size_t DecodeFrameCount() override;
  void InitializeNewFrame(size_t) override;
  void Decode(size_t) override;
  bool CanReusePreviousFrameBuffer(size_t) const override;

  // Creates |decoder_| and decodes the size and frame count.
  bool MaybeCreateDemuxer();

  // Decodes the frame at index |index|. The decoded frame is available in
  // decoder_->image. Returns true on success, false on failure.
  bool DecodeImage(size_t index);

  // Updates or creates |color_transform_|. Returns true on success, false on
  // failure.
  bool UpdateColorTransform(const gfx::ColorSpace& src_cs,
                            const gfx::ColorSpace& dest_cs);

  // Returns true if we can set the color space on the image.
  bool CanSetColorSpace() const;

  // Renders |image| in |buffer|. |frame_cs| is the color space of |image|.
  // Returns true on success, false on failure.
  bool RenderImage(const avifImage* image,
                   const gfx::ColorSpace& frame_cs,
                   ImageFrame* buffer);

  bool is_high_bit_depth_ = false;
  bool decode_to_half_float_ = false;
  size_t decoded_frame_count_ = 0;
  std::unique_ptr<avifDecoder, void (*)(avifDecoder*)> decoder_{nullptr,
                                                                nullptr};

  std::unique_ptr<gfx::ColorTransform> color_transform_;

  gfx::ColorSpace last_color_space_;
  sk_sp<SkData> image_data_;
  SkBitmap temp_bitmap_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_AVIF_AVIF_IMAGE_DECODER_H_
