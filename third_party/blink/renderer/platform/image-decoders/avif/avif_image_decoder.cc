// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/avif/avif_image_decoder.h"

#include <memory>

#include "base/containers/adapters.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/ranges.h"
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "media/base/video_color_space.h"
#include "media/base/video_frame.h"
#include "media/renderers/paint_canvas_video_renderer.h"
#include "third_party/blink/renderer/platform/image-decoders/fast_shared_buffer_reader.h"
#include "third_party/blink/renderer/platform/image-decoders/image_animation.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/libavif/src/include/avif/avif.h"
#include "third_party/libyuv/include/libyuv.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/half_float.h"
#include "ui/gfx/icc_profile.h"

#if defined(ARCH_CPU_BIG_ENDIAN)
#error Blink assumes a little-endian target.
#endif

namespace {

gfx::ColorSpace GetColorSpace(const avifImage* image) {
  if (image->profileFormat == AVIF_PROFILE_FORMAT_NCLX) {
    media::VideoColorSpace color_space(image->nclx.colourPrimaries,
                                       image->nclx.transferCharacteristics,
                                       image->nclx.matrixCoefficients,
                                       image->nclx.range == AVIF_RANGE_FULL
                                           ? gfx::ColorSpace::RangeID::FULL
                                           : gfx::ColorSpace::RangeID::LIMITED);
    if (color_space.IsSpecified())
      return color_space.ToGfxColorSpace();
    if (image->nclx.range == AVIF_RANGE_FULL)
      return gfx::ColorSpace::CreateJpeg();
    return gfx::ColorSpace::CreateREC709();
  }
  if (image->profileFormat == AVIF_PROFILE_FORMAT_ICC) {
    auto iccp = gfx::ICCProfile::FromData(image->icc.data, image->icc.size);
    if (iccp.IsValid())
      return iccp.GetColorSpace();

    // TODO(dalecurtis): Do we need to reparse this per frame when dealing
    // with animated AVIF files? Or is it only for still picture?

    // TODO(wtc): We need to set the color profile using
    // SetEmbeddedColorProfile() rather than handling all the color space
    // conversion during decode.
  }
  return gfx::ColorSpace();
}

media::VideoPixelFormat AvifToVideoPixelFormat(avifPixelFormat fmt, int depth) {
  if (depth != 8 && depth != 10 && depth != 12) {
    // Unsupported bit depth.
    NOTREACHED();
    return media::PIXEL_FORMAT_UNKNOWN;
  }
  int index = (depth - 8) / 2;
  static constexpr media::VideoPixelFormat kYUV420Formats[] = {
      media::PIXEL_FORMAT_I420, media::PIXEL_FORMAT_YUV420P10,
      media::PIXEL_FORMAT_YUV420P12};
  static constexpr media::VideoPixelFormat kYUV422Formats[] = {
      media::PIXEL_FORMAT_I422, media::PIXEL_FORMAT_YUV422P10,
      media::PIXEL_FORMAT_YUV422P12};
  static constexpr media::VideoPixelFormat kYUV444Formats[] = {
      media::PIXEL_FORMAT_I444, media::PIXEL_FORMAT_YUV444P10,
      media::PIXEL_FORMAT_YUV444P12};
  switch (fmt) {
    case AVIF_PIXEL_FORMAT_YUV420:
      return kYUV420Formats[index];
    case AVIF_PIXEL_FORMAT_YUV422:
      return kYUV422Formats[index];
    case AVIF_PIXEL_FORMAT_YUV444:
      return kYUV444Formats[index];
    case AVIF_PIXEL_FORMAT_YV12:
      NOTIMPLEMENTED();
      return media::PIXEL_FORMAT_UNKNOWN;
    case AVIF_PIXEL_FORMAT_NONE:
      NOTREACHED();
      return media::PIXEL_FORMAT_UNKNOWN;
  }
}

inline void WritePixel(float max_channel,
                       const gfx::Point3F& pixel,
                       int alpha,
                       uint32_t* rgba_dest) {
  *rgba_dest = SkPackARGB32NoCheck(
      alpha,
      gfx::ToRoundedInt(base::ClampToRange(pixel.x(), 0.0f, 1.0f) *
                        max_channel),
      gfx::ToRoundedInt(base::ClampToRange(pixel.y(), 0.0f, 1.0f) *
                        max_channel),
      gfx::ToRoundedInt(base::ClampToRange(pixel.z(), 0.0f, 1.0f) *
                        max_channel));
}

inline void WritePixel(float max_channel,
                       const gfx::Point3F& pixel,
                       int alpha,
                       uint64_t* rgba_dest) {
  float rgba_pixels[4];
  rgba_pixels[0] = pixel.x();
  rgba_pixels[1] = pixel.y();
  rgba_pixels[2] = pixel.z();
  rgba_pixels[3] = alpha / max_channel;

  gfx::FloatToHalfFloat(rgba_pixels, reinterpret_cast<uint16_t*>(rgba_dest),
                        base::size(rgba_pixels));
}

enum class ColorType { kMono, kColor };

template <ColorType color_type, typename InputType, typename OutputType>
void YUVAToRGBA(const avifImage* image,
                const gfx::ColorTransform* transform,
                OutputType* rgba_dest) {
  avifPixelFormatInfo format_info;
  avifGetPixelFormatInfo(image->yuvFormat, &format_info);
  gfx::Point3F pixel;
  const int max_channel_i = (1 << image->depth) - 1;
  const float max_channel = float{max_channel_i};
  for (uint32_t j = 0; j < image->height; ++j) {
    const int uv_j = j >> format_info.chromaShiftY;

    const InputType* y_ptr = reinterpret_cast<InputType*>(
        &image->yuvPlanes[AVIF_CHAN_Y][j * image->yuvRowBytes[AVIF_CHAN_Y]]);
    const InputType* u_ptr = reinterpret_cast<InputType*>(
        &image->yuvPlanes[AVIF_CHAN_U][uv_j * image->yuvRowBytes[AVIF_CHAN_U]]);
    const InputType* v_ptr = reinterpret_cast<InputType*>(
        &image->yuvPlanes[AVIF_CHAN_V][uv_j * image->yuvRowBytes[AVIF_CHAN_V]]);
    const InputType* a_ptr = nullptr;
    if (image->alphaPlane) {
      a_ptr = reinterpret_cast<InputType*>(
          &image->alphaPlane[j * image->alphaRowBytes]);
    }

    for (uint32_t i = 0; i < image->width; ++i) {
      const int uv_i = i >> format_info.chromaShiftX;
      // TODO(wtc): Use templates or other ways to avoid doing this comparison
      // and checking whether the image supports alpha in the inner loop.
      if (image->yuvRange == AVIF_RANGE_LIMITED) {
        pixel.set_x(avifLimitedToFullY(image->depth, y_ptr[i]) / max_channel);
        if (color_type == ColorType::kColor) {
          pixel.set_y(avifLimitedToFullUV(image->depth, u_ptr[uv_i]) /
                      max_channel);
          pixel.set_z(avifLimitedToFullUV(image->depth, v_ptr[uv_i]) /
                      max_channel);
        }
      } else {
        pixel.set_x(y_ptr[i] / max_channel);
        if (color_type == ColorType::kColor) {
          pixel.set_y(u_ptr[uv_i] / max_channel);
          pixel.set_z(v_ptr[uv_i] / max_channel);
        }
      }
      if (color_type == ColorType::kMono) {
        pixel.set_y(0.5f);
        pixel.set_z(0.5f);
      }

      transform->Transform(&pixel, 1);

      int alpha = max_channel_i;
      if (a_ptr) {
        alpha = a_ptr[i];
        if (image->alphaRange == AVIF_RANGE_LIMITED)
          alpha = avifLimitedToFullY(image->depth, alpha);
      }

      WritePixel(max_channel, pixel, alpha, rgba_dest);
      rgba_dest++;
    }
  }
}

}  // namespace

namespace blink {

AVIFImageDecoder::AVIFImageDecoder(AlphaOption alpha_option,
                                   HighBitDepthDecodingOption hbd_option,
                                   const ColorBehavior& color_behavior,
                                   size_t max_decoded_bytes)
    : ImageDecoder(alpha_option,
                   hbd_option,
                   color_behavior,
                   max_decoded_bytes) {}

AVIFImageDecoder::~AVIFImageDecoder() = default;

bool AVIFImageDecoder::ImageIsHighBitDepth() {
  return is_high_bit_depth_;
}

void AVIFImageDecoder::OnSetData(SegmentReader* data) {
  // avifDecoder requires all the data be available before reading and cannot
  // read incrementally as data comes in. See
  // https://github.com/AOMediaCodec/libavif/issues/11.
  if (IsAllDataReceived() && !MaybeCreateDemuxer())
    SetFailed();
}

int AVIFImageDecoder::RepetitionCount() const {
  return decoded_frame_count_ > 1 ? kAnimationLoopInfinite : kAnimationNone;
}

base::TimeDelta AVIFImageDecoder::FrameDurationAtIndex(size_t index) const {
  return index < frame_buffer_cache_.size()
             ? frame_buffer_cache_[index].Duration()
             : base::TimeDelta();
}

// static
bool AVIFImageDecoder::MatchesAVIFSignature(
    const FastSharedBufferReader& fast_reader) {
  // avifPeekCompatibleFileType() clamps compatible brands at 32 when reading in
  // the ftyp box in ISOBMFF for the 'av01' brand. So the maximum number of
  // bytes read is 144 bytes (type 4 bytes, size 4 bytes, major brand 4 bytes,
  // version 4 bytes, and 4 bytes * 32 compatible brands).
  char buffer[144];
  avifROData input;
  input.size = std::min(sizeof(buffer), fast_reader.size());
  input.data = reinterpret_cast<const uint8_t*>(
      fast_reader.GetConsecutiveData(0, input.size, buffer));
  return avifPeekCompatibleFileType(&input);
}

void AVIFImageDecoder::DecodeSize() {
  // Because avifDecoder cannot read incrementally as data comes in, we cannot
  // decode the size until all data is received. When all data is received,
  // OnSetData() decodes the size right away. So DecodeSize() doesn't need to do
  // anything.
}

size_t AVIFImageDecoder::DecodeFrameCount() {
  return decoded_frame_count_;
}

void AVIFImageDecoder::InitializeNewFrame(size_t index) {
  auto& buffer = frame_buffer_cache_[index];

  buffer.SetOriginalFrameRect(IntRect(IntPoint(), Size()));

  avifImageTiming timing;
  auto ret = avifDecoderNthImageTiming(decoder_.get(), index, &timing);
  DCHECK_EQ(ret, AVIF_RESULT_OK);
  buffer.SetDuration(base::TimeDelta::FromSecondsD(timing.duration));

  // The AVIF file format does not contain information equivalent to the
  // disposal method or alpha blend source. Since the AVIF decoder handles frame
  // dependence internally, set options that best correspond to "each frame is
  // independent".
  buffer.SetDisposalMethod(ImageFrame::kDisposeNotSpecified);
  buffer.SetAlphaBlendSource(ImageFrame::kBlendAtopBgcolor);

  if (decode_to_half_float_)
    buffer.SetPixelFormat(ImageFrame::PixelFormat::kRGBA_F16);

  // Leave all frames as being independent (the default) because we require all
  // frames be the same size.
  DCHECK_EQ(buffer.RequiredPreviousFrameIndex(), kNotFound);
}

void AVIFImageDecoder::Decode(size_t index) {
  // TODO(dalecurtis): For fragmented avif-sequence files we probably want to
  // allow partial decoding. Depends on if we see frequent use of multi-track
  // images where there's lots to ignore.
  if (Failed() || !IsAllDataReceived())
    return;

  if (!DecodeImage(index)) {
    SetFailed();
    return;
  }

  const auto* image = decoder_->image;
  // All frames must be the same size.
  if (Size() != IntSize(image->width, image->height)) {
    SetFailed();
    return;
  }

  ImageFrame& buffer = frame_buffer_cache_[index];
  DCHECK_NE(buffer.GetStatus(), ImageFrame::kFrameComplete);
  buffer.SetHasAlpha(!!image->alphaPlane);
  if (decode_to_half_float_)
    buffer.SetPixelFormat(ImageFrame::PixelFormat::kRGBA_F16);

  // Set color space information on the frame if appropriate.
  gfx::ColorSpace frame_cs;
  if (!IgnoresColorSpace())
    frame_cs = GetColorSpace(image);
  if (CanSetColorSpace()) {
    last_color_space_ = frame_cs.GetAsFullRangeRGB();
  } else {
    // Just use whatever color space Skia wants us to use.
  }

  // TODO(wtc): This should use the value of |last_color_space_|. Implement it.
  if (!InitFrameBuffer(index)) {
    DVLOG(1) << "Failed to create frame buffer...";
    SetFailed();
    return;
  }

  if (!RenderImage(image, frame_cs, &buffer)) {
    SetFailed();
    return;
  }

  buffer.SetPixelsChanged(true);
  buffer.SetStatus(ImageFrame::kFrameComplete);
}

bool AVIFImageDecoder::CanReusePreviousFrameBuffer(size_t index) const {
  // (a) Technically we can reuse the bitmap of the previous frame because the
  // AVIF decoder handles frame dependence internally and we never need to
  // preserve previous frames to decode later ones, and (b) since this function
  // will not currently be called, this is really more for the reader than any
  // functional purpose.
  return true;
}

bool AVIFImageDecoder::MaybeCreateDemuxer() {
  if (decoder_)
    return true;

  decoder_ = std::unique_ptr<avifDecoder, void (*)(avifDecoder*)>(
      avifDecoderCreate(), avifDecoderDestroy);
  if (!decoder_)
    return false;

  // TODO(dalecurtis): This may create a second copy of the media data in
  // memory, which is not great. Upstream should provide a read() based API:
  // https://github.com/AOMediaCodec/libavif/issues/11
  image_data_ = data_->GetAsSkData();
  if (!image_data_)
    return false;

  avifROData raw_data = {image_data_->bytes(), image_data_->size()};
  auto ret = avifDecoderParse(decoder_.get(), &raw_data);
  if (ret != AVIF_RESULT_OK) {
    DVLOG(1) << "avifDecoderParse failed: " << avifResultToString(ret);
    return false;
  }

  DCHECK_GT(decoder_->imageCount, 0);
  decoded_frame_count_ = decoder_->imageCount;
  is_high_bit_depth_ = decoder_->containerDepth > 8;
  decode_to_half_float_ =
      is_high_bit_depth_ &&
      high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat;

  // Try to get the size from the container if possible instead of decoding.
  if (decoder_->containerWidth && decoder_->containerHeight)
    return SetSize(decoder_->containerWidth, decoder_->containerHeight);

  // We need to SetSize() to proceed, so decode the first frame.
  return DecodeImage(0) &&
         SetSize(decoder_->image->width, decoder_->image->height);
}

bool AVIFImageDecoder::DecodeImage(size_t index) {
  auto ret = avifDecoderNthImage(decoder_.get(), index);
  if (ret != AVIF_RESULT_OK) {
    // We shouldn't be called more times than specified in
    // DecodeFrameCount(); possibly this should truncate if the initial
    // count is wrong?
    DCHECK_NE(ret, AVIF_RESULT_NO_IMAGES_REMAINING);
    return false;
  }

  const auto* image = decoder_->image;
  is_high_bit_depth_ = image->depth > 8;
  decode_to_half_float_ =
      is_high_bit_depth_ &&
      high_bit_depth_decoding_option_ == kHighBitDepthToHalfFloat;
  return true;
}

bool AVIFImageDecoder::UpdateColorTransform(const gfx::ColorSpace& src_cs,
                                            const gfx::ColorSpace& dest_cs) {
  if (color_transform_ && color_transform_->GetSrcColorSpace() == src_cs)
    return true;
  color_transform_ = gfx::ColorTransform::NewColorTransform(
      src_cs, dest_cs, gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);
  return !!color_transform_;
}

// TODO(wtc): We must be able to set the color space accurately. Find a solution
// that lets us set the color space for all images and not just the half float
// and animated cases.
bool AVIFImageDecoder::CanSetColorSpace() const {
  return decode_to_half_float_ || decoded_frame_count_ > 1;
}

bool AVIFImageDecoder::RenderImage(const avifImage* image,
                                   const gfx::ColorSpace& frame_cs,
                                   ImageFrame* buffer) {
  const gfx::ColorSpace dest_rgb_cs(*buffer->Bitmap().colorSpace());

  const bool is_mono = !image->yuvPlanes[AVIF_CHAN_U];

  // TODO(dalecurtis): We should decode to YUV when possible. Currently the
  // YUV path seems to only support still-image YUV8.
  if (decode_to_half_float_) {
    if (!UpdateColorTransform(frame_cs, dest_rgb_cs)) {
      DVLOG(1) << "Failed to update color transform...";
      return false;
    }

    uint64_t* rgba_hhhh = buffer->GetAddrF16(0, 0);

    // Color and format convert from YUV HBD -> RGBA half float.
    if (is_mono) {
      YUVAToRGBA<ColorType::kMono, uint16_t>(image, color_transform_.get(),
                                             rgba_hhhh);
    } else {
      // TODO: Add fast path for 10bit 4:2:0 using libyuv.
      YUVAToRGBA<ColorType::kColor, uint16_t>(image, color_transform_.get(),
                                              rgba_hhhh);
    }
    return true;
  }

  uint32_t* rgba_8888 = buffer->GetAddr(0, 0);
  if (ImageIsHighBitDepth() ||
      // TODO(wtc): Figure out a way to check frame_cs == ~BT.2020 too since
      // ConvertVideoFrameToRGBPixels() can handle that too.
      frame_cs == gfx::ColorSpace::CreateREC709() ||
      frame_cs == gfx::ColorSpace::CreateREC601() ||
      frame_cs == gfx::ColorSpace::CreateJpeg()) {
    // Create temporary frame wrapping the YUVA planes.
    scoped_refptr<media::VideoFrame> frame;
    auto pixel_format = AvifToVideoPixelFormat(image->yuvFormat, image->depth);
    if (pixel_format == media::PIXEL_FORMAT_UNKNOWN)
      return false;
    auto size = gfx::Size(image->width, image->height);
    if (image->alphaPlane) {
      if (pixel_format == media::PIXEL_FORMAT_I420) {
        pixel_format = media::PIXEL_FORMAT_I420A;
      } else {
        NOTIMPLEMENTED();
        return false;
      }
      frame = media::VideoFrame::WrapExternalYuvaData(
          pixel_format, size, gfx::Rect(size), size, image->yuvRowBytes[0],
          image->yuvRowBytes[1], image->yuvRowBytes[2], image->alphaRowBytes,
          image->yuvPlanes[0], image->yuvPlanes[1], image->yuvPlanes[2],
          image->alphaPlane, base::TimeDelta());
    } else {
      frame = media::VideoFrame::WrapExternalYuvData(
          pixel_format, size, gfx::Rect(size), size, image->yuvRowBytes[0],
          image->yuvRowBytes[1], image->yuvRowBytes[2], image->yuvPlanes[0],
          image->yuvPlanes[1], image->yuvPlanes[2], base::TimeDelta());
    }

    // Really only handles 709, 601, JPEG 8-bit conversions and uses libyuv
    // under the hood, so is much faster than our manual path.
    //
    // Technically has support for 10-bit 4:2:0 and 4:2:2, but not to
    // half-float and only has support for 4:4:4 and 12-bit by down-shifted
    // copies.
    //
    // https://bugs.chromium.org/p/libyuv/issues/detail?id=845
    media::PaintCanvasVideoRenderer::ConvertVideoFrameToRGBPixels(
        frame.get(), rgba_8888, frame->visible_rect().width() * 4);
    return true;
  }

  if (!UpdateColorTransform(frame_cs, dest_rgb_cs)) {
    DVLOG(1) << "Failed to update color transform...";
    return false;
  }
  if (is_mono) {
    YUVAToRGBA<ColorType::kMono, uint8_t>(image, color_transform_.get(),
                                          rgba_8888);
  } else {
    YUVAToRGBA<ColorType::kColor, uint8_t>(image, color_transform_.get(),
                                           rgba_8888);
  }
  return true;
}

}  // namespace blink
