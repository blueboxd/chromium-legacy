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

// Builds a gfx::ColorSpace from the ITU-T H.273 (CICP) color description in the
// image. This color space is used to create the gfx::ColorTransform for the
// YUV-to-RGB conversion. If the image does not have an ICC profile, this color
// space is also used to create the embedded color profile.
gfx::ColorSpace GetColorSpace(const avifImage* image) {
  // MIAF Section 7.3.6.4 says:
  //   If a coded image has no associated colour property, the default property
  //   is defined as having colour_type equal to 'nclx' with properties as
  //   follows:
  //   – For YCbCr encoding, sYCC should be assumed as indicated by
  //   colour_primaries equal to 1, transfer_characteristics equal to 13,
  //   matrix_coefficients equal to 1, and full_range_flag equal to 1.
  //   ...
  // Note that this only specifies the default color property when the color
  // property is absent. It does not really specify the default values for
  // colour_primaries, transfer_characteristics, and matrix_coefficients when
  // they are equal to 2 (unspecified). But we will interpret it as specifying
  // the default values for these variables because we must choose some defaults
  // and these are the most reasonable defaults to choose. We also advocate that
  // all AVIF decoders choose these defaults:
  // https://github.com/AOMediaCodec/av1-avif/issues/84
  const auto primaries =
      image->colorPrimaries == AVIF_COLOR_PRIMARIES_UNSPECIFIED
          ? AVIF_COLOR_PRIMARIES_BT709
          : image->colorPrimaries;
  const auto transfer = image->transferCharacteristics ==
                                AVIF_TRANSFER_CHARACTERISTICS_UNSPECIFIED
                            ? AVIF_TRANSFER_CHARACTERISTICS_SRGB
                            : image->transferCharacteristics;
  const auto matrix =
      image->matrixCoefficients == AVIF_MATRIX_COEFFICIENTS_UNSPECIFIED
          ? AVIF_MATRIX_COEFFICIENTS_BT709
          : image->matrixCoefficients;
  const auto range = image->yuvRange == AVIF_RANGE_FULL
                         ? gfx::ColorSpace::RangeID::FULL
                         : gfx::ColorSpace::RangeID::LIMITED;
  media::VideoColorSpace color_space(primaries, transfer, matrix, range);
  if (color_space.IsSpecified())
    return color_space.ToGfxColorSpace();
  // media::VideoColorSpace and gfx::ColorSpace do not support CICP
  // MatrixCoefficients 12, 13, 14.
  DCHECK_GE(matrix, 12);
  DCHECK_LE(matrix, 14);
  if (image->yuvRange == AVIF_RANGE_FULL)
    return gfx::ColorSpace::CreateJpeg();
  return gfx::ColorSpace::CreateREC709();
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
                       float alpha,
                       bool premultiply_alpha,
                       uint32_t* rgba_dest) {
  unsigned r =
      gfx::ToRoundedInt(base::ClampToRange(pixel.x(), 0.0f, 1.0f) * 255.0f);
  unsigned g =
      gfx::ToRoundedInt(base::ClampToRange(pixel.y(), 0.0f, 1.0f) * 255.0f);
  unsigned b =
      gfx::ToRoundedInt(base::ClampToRange(pixel.z(), 0.0f, 1.0f) * 255.0f);
  unsigned a = gfx::ToRoundedInt(alpha * 255.0f);
  if (premultiply_alpha)
    blink::ImageFrame::SetRGBAPremultiply(rgba_dest, r, g, b, a);
  else
    *rgba_dest = SkPackARGB32NoCheck(a, r, g, b);
}

inline void WritePixel(float max_channel,
                       const gfx::Point3F& pixel,
                       float alpha,
                       bool premultiply_alpha,
                       uint64_t* rgba_dest) {
  float rgba_pixels[4];
  rgba_pixels[0] = pixel.x();
  rgba_pixels[1] = pixel.y();
  rgba_pixels[2] = pixel.z();
  rgba_pixels[3] = alpha;
  if (premultiply_alpha && alpha != 1.0f) {
    rgba_pixels[0] *= alpha;
    rgba_pixels[1] *= alpha;
    rgba_pixels[2] *= alpha;
  }

  gfx::FloatToHalfFloat(rgba_pixels, reinterpret_cast<uint16_t*>(rgba_dest),
                        base::size(rgba_pixels));
}

enum class ColorType { kMono, kColor };

template <ColorType color_type, typename InputType, typename OutputType>
void YUVAToRGBA(const avifImage* image,
                const gfx::ColorTransform* transform,
                bool premultiply_alpha,
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

      WritePixel(max_channel, pixel, alpha / max_channel, premultiply_alpha,
                 rgba_dest);
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
  return Failed() ? frame_buffer_cache_.size() : decoded_frame_count_;
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
    DVLOG(1) << "All frames must be the same size";
    SetFailed();
    return;
  }

  ImageFrame& buffer = frame_buffer_cache_[index];
  DCHECK_NE(buffer.GetStatus(), ImageFrame::kFrameComplete);
  if (decode_to_half_float_)
    buffer.SetPixelFormat(ImageFrame::PixelFormat::kRGBA_F16);

  if (!InitFrameBuffer(index)) {
    DVLOG(1) << "Failed to create frame buffer...";
    SetFailed();
    return;
  }

  if (!RenderImage(image, &buffer)) {
    SetFailed();
    return;
  }

  ColorCorrectImage(&buffer);

  buffer.SetPixelsChanged(true);
  buffer.SetHasAlpha(!!image->alphaPlane);
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

  // SetEmbeddedColorProfile() must be called before IsSizeAvailable() becomes
  // true. So call SetEmbeddedColorProfile() before calling SetSize(). The color
  // profile is either an ICC profile or the CICP color description. The CICP
  // color description may come from either the nclx colr box in the container
  // or the AV1 sequence header for the frames. Decode the first frame to ensure
  // the CICP color description is available.
  if (!DecodeImage(0))
    return false;

  const auto* image = decoder_->image;

  if (!IgnoresColorSpace()) {
    // The CICP color description is always present because we can always get it
    // from the AV1 sequence header for the frames. If an ICC profile is
    // present, use it instead of the CICP color description.
    if (image->icc.size) {
      std::unique_ptr<ColorProfile> profile =
          ColorProfile::Create(image->icc.data, image->icc.size);
      if (!profile) {
        DVLOG(1) << "Failed to parse image ICC profile";
        return false;
      }
      uint32_t data_color_space = profile->GetProfile()->data_color_space;
      if (!image->yuvPlanes[AVIF_CHAN_U]) {
        // Monochrome (grayscale) image.
        if (data_color_space != skcms_Signature_Gray &&
            data_color_space != skcms_Signature_RGB)
          profile = nullptr;
      } else {
        if (data_color_space != skcms_Signature_RGB)
          profile = nullptr;
      }
      if (!profile) {
        DVLOG(1)
            << "Image contains ICC profile that does not match its color space";
        return false;
      }
      SetEmbeddedColorProfile(std::move(profile));
    } else {
      gfx::ColorSpace frame_cs = GetColorSpace(image);
      sk_sp<SkColorSpace> sk_color_space =
          frame_cs.GetAsFullRangeRGB().ToSkColorSpace();
      skcms_ICCProfile profile;
      sk_color_space->toProfile(&profile);
      SetEmbeddedColorProfile(std::make_unique<ColorProfile>(profile));
    }
  }

  // The size from the container, if present, must be the same as the first
  // frame's size.
  if (decoder_->containerWidth && decoder_->containerHeight &&
      (decoder_->containerWidth != image->width ||
       decoder_->containerHeight != image->height)) {
    DVLOG(1) << "Container size and image size disagree";
    return false;
  }

  return SetSize(image->width, image->height);
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

void AVIFImageDecoder::UpdateColorTransform(const gfx::ColorSpace& frame_cs) {
  DCHECK_EQ(frame_cs.GetRangeID(), gfx::ColorSpace::RangeID::FULL);
  if (color_transform_ && color_transform_->GetSrcColorSpace() == frame_cs)
    return;
  color_transform_ = gfx::ColorTransform::NewColorTransform(
      frame_cs, frame_cs.GetAsFullRangeRGB(),
      gfx::ColorTransform::Intent::INTENT_PERCEPTUAL);
}

bool AVIFImageDecoder::RenderImage(const avifImage* image, ImageFrame* buffer) {
  const gfx::ColorSpace frame_cs = GetColorSpace(image);
  // Although gfx::ColorTransform can perform range adjustment (from limited
  // range to full range), it uses the 8-bit equations for all bit depths, which
  // are not very accurate for high bit depths. So YUVAToRGBA() performs range
  // adjustment (using libavif) before calling gfx::ColorTransform::Transform().
  // Therefore, the source color space passed to UpdateColorTransform() should
  // be full range.
  const gfx::ColorSpace frame_cs_full_range = frame_cs.GetWithMatrixAndRange(
      frame_cs.GetMatrixID(), gfx::ColorSpace::RangeID::FULL);

  const bool is_mono = !image->yuvPlanes[AVIF_CHAN_U];
  const bool premultiply_alpha = buffer->PremultiplyAlpha();

  // TODO(dalecurtis): We should decode to YUV when possible. Currently the
  // YUV path seems to only support still-image YUV8.
  if (decode_to_half_float_) {
    UpdateColorTransform(frame_cs_full_range);

    uint64_t* rgba_hhhh = buffer->GetAddrF16(0, 0);

    // Color and format convert from YUV HBD -> RGBA half float.
    if (is_mono) {
      YUVAToRGBA<ColorType::kMono, uint16_t>(image, color_transform_.get(),
                                             premultiply_alpha, rgba_hhhh);
    } else {
      // TODO: Add fast path for 10bit 4:2:0 using libyuv.
      YUVAToRGBA<ColorType::kColor, uint16_t>(image, color_transform_.get(),
                                              premultiply_alpha, rgba_hhhh);
    }
    return true;
  }

  uint32_t* rgba_8888 = buffer->GetAddr(0, 0);
  // libyuv supports the alpha channel only with the I420 pixel format. libavif
  // reports monochrome 4:0:0 as AVIF_PIXEL_FORMAT_YUV420 with null U and V
  // planes, so we need to check for genuine YUV 4:2:0, not monochrome 4:0:0.
  bool is_i420 = image->depth == 8 &&
                 image->yuvFormat == AVIF_PIXEL_FORMAT_YUV420 && !is_mono;
  // Call media::PaintCanvasVideoRenderer if the color space is supported by
  // libyuv. Since media::PaintCanvasVideoRenderer calls
  // libyuv::I420AlphaToARGB() with attenuate=1 to enable RGB premultiplication
  // by alpha, we need to check both is_i420 and premultiply_alpha.
  // TODO(wtc): Figure out a way to check frame_cs == ~BT.2020 too since
  // ConvertVideoFrameToRGBPixels() can handle that too.
  if ((frame_cs == gfx::ColorSpace::CreateREC709() ||
       frame_cs == gfx::ColorSpace::CreateREC601() ||
       frame_cs == gfx::ColorSpace::CreateJpeg()) &&
      (!image->alphaPlane || (is_i420 && premultiply_alpha))) {
    // Create temporary frame wrapping the YUVA planes.
    scoped_refptr<media::VideoFrame> frame;
    auto pixel_format = AvifToVideoPixelFormat(image->yuvFormat, image->depth);
    if (pixel_format == media::PIXEL_FORMAT_UNKNOWN)
      return false;
    auto size = gfx::Size(image->width, image->height);
    if (image->alphaPlane) {
      if (is_i420) {
        DCHECK_EQ(pixel_format, media::PIXEL_FORMAT_I420);
        pixel_format = media::PIXEL_FORMAT_I420A;
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
    frame->set_color_space(frame_cs);

    // Really only handles 709, 601, 2020, JPEG 8-bit conversions and uses
    // libyuv under the hood, so is much faster than our manual path.
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

  UpdateColorTransform(frame_cs_full_range);
  if (ImageIsHighBitDepth()) {
    if (is_mono) {
      YUVAToRGBA<ColorType::kMono, uint16_t>(image, color_transform_.get(),
                                             premultiply_alpha, rgba_8888);
    } else {
      YUVAToRGBA<ColorType::kColor, uint16_t>(image, color_transform_.get(),
                                              premultiply_alpha, rgba_8888);
    }
  } else {
    if (is_mono) {
      YUVAToRGBA<ColorType::kMono, uint8_t>(image, color_transform_.get(),
                                            premultiply_alpha, rgba_8888);
    } else {
      YUVAToRGBA<ColorType::kColor, uint8_t>(image, color_transform_.get(),
                                             premultiply_alpha, rgba_8888);
    }
  }
  return true;
}

void AVIFImageDecoder::ColorCorrectImage(ImageFrame* buffer) {
  // Postprocess the image data according to the profile.
  const ColorProfileTransform* const transform = ColorTransform();
  if (!transform)
    return;
  const auto alpha_format = (buffer->HasAlpha() && buffer->PremultiplyAlpha())
                                ? skcms_AlphaFormat_PremulAsEncoded
                                : skcms_AlphaFormat_Unpremul;
  if (decode_to_half_float_) {
    const skcms_PixelFormat color_format = skcms_PixelFormat_RGBA_hhhh;
    for (int y = 0; y < Size().Height(); ++y) {
      ImageFrame::PixelDataF16* const row = buffer->GetAddrF16(0, y);
      const bool success = skcms_Transform(
          row, color_format, alpha_format, transform->SrcProfile(), row,
          color_format, alpha_format, transform->DstProfile(), Size().Width());
      DCHECK(success);
    }
  } else {
    const skcms_PixelFormat color_format = XformColorFormat();
    for (int y = 0; y < Size().Height(); ++y) {
      ImageFrame::PixelData* const row = buffer->GetAddr(0, y);
      const bool success = skcms_Transform(
          row, color_format, alpha_format, transform->SrcProfile(), row,
          color_format, alpha_format, transform->DstProfile(), Size().Width());
      DCHECK(success);
    }
  }
}

}  // namespace blink
