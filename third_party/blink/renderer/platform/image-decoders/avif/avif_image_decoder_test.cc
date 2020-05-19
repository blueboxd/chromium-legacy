// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/avif/avif_image_decoder.h"

#include <cmath>
#include <memory>
#include <ostream>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/libavif/src/include/avif/avif.h"

#define FIXME_CRASH_IF_COLOR_TRANSFORMATION_IS_ENABLED 0
#define FIXME_SUPPORT_ICC_PROFILE_NO_TRANSFORM 0
#define FIXME_SUPPORT_ICC_PROFILE_TRANSFORM 0
#define FIXME_DISTINGUISH_LOSSY_OR_LOSSLESS 0
#define FIXME_CRASH_IF_COLOR_BEHAVIOR_IS_IGNORE 0

namespace blink {

namespace {

std::unique_ptr<ImageDecoder> CreateAVIFDecoder(
    ImageDecoder::AlphaOption alpha_option,
    ImageDecoder::HighBitDepthDecodingOption high_bit_depth_option,
    ColorBehavior color_behavior) {
  return std::make_unique<AVIFImageDecoder>(
      alpha_option, high_bit_depth_option, color_behavior,
      ImageDecoder::kNoDecodedImageByteLimit);
}

std::unique_ptr<ImageDecoder> CreateAVIFDecoder() {
  return CreateAVIFDecoder(ImageDecoder::kAlphaNotPremultiplied,
                           ImageDecoder::kDefaultBitDepth,
                           ColorBehavior::Tag());
}

struct ExpectedColor {
  gfx::Point point;
  SkColor color;
};

enum class ColorType {
  kRgb,
  kRgbA,
  kMono,
  kMonoA,
};

struct StaticColorCheckParam {
  const char* path;
  int bit_depth;
  ColorType color_type;
  ImageDecoder::CompressionFormat compression_format;
  ImageDecoder::AlphaOption alpha_option;
  ColorBehavior color_behavior;
  int color_threshold;
  std::vector<ExpectedColor> colors;
};

std::ostream& operator<<(std::ostream& os, const StaticColorCheckParam& param) {
  const char* color_type;
  switch (param.color_type) {
    case ColorType::kRgb:
      color_type = "kRgb";
      break;
    case ColorType::kRgbA:
      color_type = "kRgbA";
      break;
    case ColorType::kMono:
      color_type = "kMono";
      break;
    case ColorType::kMonoA:
      color_type = "kMonoA";
      break;
  }
  const char* alpha_option =
      (param.alpha_option == ImageDecoder::kAlphaPremultiplied
           ? "kAlphaPremultiplied"
           : "kAlphaNotPremultiplied");
  const char* color_behavior;
  if (param.color_behavior.IsIgnore()) {
    color_behavior = "Ignore";
  } else if (param.color_behavior.IsTag()) {
    color_behavior = "Tag";
  } else {
    DCHECK(param.color_behavior.IsTransformToSRGB());
    color_behavior = "TransformToSRGB";
  }
  return os << "\nStaticColorCheckParam {\n  path: \"" << param.path
            << "\",\n  bit_depth: " << param.bit_depth
            << ",\n  color_type: " << color_type
            << ",\n  alpha_option: " << alpha_option
            << ",\n  color_behavior: " << color_behavior << "\n}";
}

StaticColorCheckParam kTestParams[] = {
    {
        "/images/resources/avif/red-at-12-oclock-with-color-profile-lossy.avif",
        8,
        ColorType::kRgb,
        ImageDecoder::kLossyFormat,
        ImageDecoder::kAlphaNotPremultiplied,  // q=60(lossy)
        ColorBehavior::Tag(),
        0,
        {},  // we just check that this image is lossy.
    },
#if FIXME_CRASH_IF_COLOR_BEHAVIOR_IS_IGNORE
    {
        "/images/resources/avif/red-at-12-oclock-with-color-profile-lossy.avif",
        8,
        ColorType::kRgb,
        ImageDecoder::kLossyFormat,
        ImageDecoder::kAlphaNotPremultiplied,  // q=60(lossy)
        ColorBehavior::Ignore(),
        0,
        {},  // we just check that the decoder won't crash when
             // ColorBehavior::Ignore() is used.
    },
#endif
    {"/images/resources/avif/red-with-alpha-8bpc.avif",
     8,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-with-limited-alhpa-8bpc.avif",
     8,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-full-ranged-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/alpha-mask-limited-ranged-8bpc.avif",
     8,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
    {"/images/resources/avif/alpha-mask-full-ranged-8bpc.avif",
     8,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
#if FIXME_CRASH_IF_COLOR_TRANSFORMATION_IS_ENABLED
    {"/images/resources/avif/red-with-alpha-8bpc.avif",
     8,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaPremultiplied,
     ColorBehavior::TransformToSRGB(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#endif
#if FIXME_SUPPORT_ICC_PROFILE_NO_TRANSFORM && \
    FIXME_CRASH_IF_COLOR_BEHAVIOR_IS_IGNORE
    {"/images/resources/avif/red-with-profile-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Ignore(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 0, 0, 255)},
     }},
#endif
#if FIXME_SUPPORT_ICC_PROFILE_TRANSFORM
    {"/images/resources/avif/red-with-profile-8bpc.avif",
     8,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::TransformToSRGB(),
     1,
     {
         /*
          * "Color Spin" ICC profile, embedded in this image,
          * changes blue to red.
          */
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#endif
    {"/images/resources/avif/red-with-alpha-10bpc.avif",
     10,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-with-limited-alhpa-10bpc.avif",
     10,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#if FIXME_CRASH_IF_COLOR_TRANSFORMATION_IS_ENABLED
    {"/images/resources/avif/red-with-alpha-10bpc.avif",
     10,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaPremultiplied,
     ColorBehavior::TransformToSRGB(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#endif
    {"/images/resources/avif/red-full-ranged-10bpc.avif",
     10,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/alpha-mask-limited-ranged-10bpc.avif",
     10,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
    {"/images/resources/avif/alpha-mask-full-ranged-10bpc.avif",
     10,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
#if FIXME_SUPPORT_ICC_PROFILE_NO_TRANSFORM && \
    FIXME_CRASH_IF_COLOR_BEHAVIOR_IS_IGNORE
    {"/images/resources/avif/red-with-profile-10bpc.avif",
     10,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Ignore(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 0, 0, 255)},
     }},
#endif
#if FIXME_SUPPORT_ICC_PROFILE_TRANSFORM
    {"/images/resources/avif/red-with-profile-10bpc.avif",
     10,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::TransformToSRGB(),
     1,
     {
         /*
          * "Color Spin" ICC profile, embedded in this image,
          * changes blue to red.
          */
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#endif
    {"/images/resources/avif/red-with-alpha-12bpc.avif",
     12,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/red-with-limited-alhpa-12bpc.avif",
     12,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#if FIXME_CRASH_IF_COLOR_TRANSFORMATION_IS_ENABLED
    {"/images/resources/avif/red-with-alpha-12bpc.avif",
     12,
     ColorType::kRgbA,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaPremultiplied,
     ColorBehavior::TransformToSRGB(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(0, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(128, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#endif
    {"/images/resources/avif/red-full-ranged-12bpc.avif",
     12,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     0,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
    {"/images/resources/avif/alpha-mask-limited-ranged-12bpc.avif",
     12,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
    {"/images/resources/avif/alpha-mask-full-ranged-12bpc.avif",
     12,
     ColorType::kMono,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Tag(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 128, 128, 128)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 255, 255)},
     }},
#if FIXME_SUPPORT_ICC_PROFILE_NO_TRANSFORM && \
    FIXME_CRASH_IF_COLOR_BEHAVIOR_IS_IGNORE
    {"/images/resources/avif/red-with-profile-12bpc.avif",
     12,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::Ignore(),
     1,
     {
         {gfx::Point(0, 0), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 0, 0, 255)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 0, 0, 255)},
     }},
#endif
#if FIXME_SUPPORT_ICC_PROFILE_TRANSFORM
    {"/images/resources/avif/red-with-profile-12bpc.avif",
     12,
     ColorType::kRgb,
     ImageDecoder::kLosslessFormat,
     ImageDecoder::kAlphaNotPremultiplied,
     ColorBehavior::TransformToSRGB(),
     1,
     {
         /*
          * "Color Spin" ICC profile, embedded in this image,
          * changes blue to red.
          */
         {gfx::Point(0, 0), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(1, 1), SkColorSetARGB(255, 255, 0, 0)},
         {gfx::Point(2, 2), SkColorSetARGB(255, 255, 0, 0)},
     }},
#endif
    // TODO(ryoh): Add other color profile images, such as BT2020CL,
    //  BT2020NCL, Rec601, SMPTE 274M
    // TODO(ryoh): Add images with different combinations of ColorPrimaries,
    //  TransferFunction and MatrixCoefficients,
    //  such as:
    //   sRGB ColorPrimaries, BT.2020 TransferFunction and
    //   BT.609 MatrixCoefficients
    // TODO(ryoh): Add YUV422/444 tests.
    // TODO(ryoh): Add Mono + Alpha Images.
};

enum class ErrorPhase { kParse, kDecode };

// If 'error_phase' is ErrorPhase::kParse, error is expected during parse
// (SetData() call); else error is expected during decode
// (DecodeFrameBufferAtIndex() call).
void TestInvalidStaticImage(const char* avif_file, ErrorPhase error_phase) {
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder();

  scoped_refptr<SharedBuffer> data = ReadFile(avif_file);
  ASSERT_TRUE(data.get());
  decoder->SetData(data.get(), true);

  if (error_phase == ErrorPhase::kParse) {
    EXPECT_TRUE(decoder->Failed());
    EXPECT_EQ(0u, decoder->FrameCount());
    EXPECT_FALSE(decoder->DecodeFrameBufferAtIndex(0));
  } else {
    EXPECT_FALSE(decoder->Failed());
    EXPECT_GT(decoder->FrameCount(), 0u);
    ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
    ASSERT_TRUE(frame);
    EXPECT_NE(ImageFrame::kFrameComplete, frame->GetStatus());
  }
  EXPECT_TRUE(decoder->Failed());
}

}  // namespace

TEST(AnimatedAVIFTests, ValidImages) {
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/star-8bpc.avifs", 5u,
                       kAnimationLoopInfinite);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/star-8bpc-with-alpha.avifs", 5u,
                       kAnimationLoopInfinite);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/star-10bpc.avifs", 5u,
                       kAnimationLoopInfinite);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/star-10bpc-with-alpha.avifs", 5u,
                       kAnimationLoopInfinite);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/star-12bpc.avifs", 5u,
                       kAnimationLoopInfinite);
  TestByteByByteDecode(&CreateAVIFDecoder,
                       "/images/resources/avif/star-12bpc-with-alpha.avifs", 5u,
                       kAnimationLoopInfinite);
  // TODO(ryoh): Add avifs with EditListBox.
}

// TODO(ryoh): Add corrupted video tests.

TEST(StaticAVIFTests, invalidImages) {
  // Image data is truncated.
  TestInvalidStaticImage(
      "/images/resources/avif/"
      "red-at-12-oclock-with-color-profile-truncated.avif",
      ErrorPhase::kParse);
  // Chunk size in AV1 frame header doesn't match the file size.
  TestInvalidStaticImage(
      "/images/resources/avif/"
      "red-at-12-oclock-with-color-profile-with-wrong-frame-header.avif",
      ErrorPhase::kDecode);
}

TEST(StaticAVIFTests, ValidImages) {
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/red-at-12-oclock-with-color-profile-lossy.avif",
      1, kAnimationNone);
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/red-at-12-oclock-with-color-profile-8bpc.avif", 1,
      kAnimationNone);
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/red-at-12-oclock-with-color-profile-10bpc.avif",
      1, kAnimationNone);
  TestByteByByteDecode(
      &CreateAVIFDecoder,
      "/images/resources/avif/red-at-12-oclock-with-color-profile-12bpc.avif",
      1, kAnimationNone);
}

using StaticAVIFColorTests = ::testing::TestWithParam<StaticColorCheckParam>;

INSTANTIATE_TEST_CASE_P(Parameterized,
                        StaticAVIFColorTests,
                        ::testing::ValuesIn(kTestParams));

TEST_P(StaticAVIFColorTests, InspectImage) {
  const StaticColorCheckParam& param = GetParam();
  // TODO(ryoh): Add tests with ImageDecoder::kHighBitDepthToHalfFloat
  std::unique_ptr<ImageDecoder> decoder = CreateAVIFDecoder(
      param.alpha_option, ImageDecoder::kDefaultBitDepth, param.color_behavior);
  scoped_refptr<SharedBuffer> data = ReadFile(param.path);
  ASSERT_TRUE(data.get());
#if FIXME_DISTINGUISH_LOSSY_OR_LOSSLESS
  EXPECT_EQ(param.compression_format,
            ImageDecoder::GetCompressionFormat(data, "image/avif"));
#endif
  decoder->SetData(data.get(), true);
  EXPECT_EQ(1u, decoder->FrameCount());
  EXPECT_EQ(kAnimationNone, decoder->RepetitionCount());
  ImageFrame* frame = decoder->DecodeFrameBufferAtIndex(0);
  ASSERT_TRUE(frame);
  EXPECT_EQ(ImageFrame::kFrameComplete, frame->GetStatus());
  EXPECT_FALSE(decoder->Failed());
  EXPECT_EQ(param.bit_depth > 8, decoder->ImageIsHighBitDepth());
  // TODO(ryoh): How should we treat imir(mirroring), irot(rotation) and
  // clap(cropping)?
  // EXPECT_EQ(xxxx, decoder->Orientation());
  EXPECT_EQ(param.color_type == ColorType::kRgbA ||
                param.color_type == ColorType::kMonoA,
            frame->HasAlpha());
  auto get_color_channel = [](SkColorChannel channel, SkColor color) {
    switch (channel) {
      case SkColorChannel::kR:
        return SkColorGetR(color);
      case SkColorChannel::kG:
        return SkColorGetG(color);
      case SkColorChannel::kB:
        return SkColorGetB(color);
      case SkColorChannel::kA:
        return SkColorGetA(color);
    }
  };
  auto color_difference = [get_color_channel](SkColorChannel channel,
                                              SkColor color1,
                                              SkColor color2) -> int {
    return std::abs(static_cast<int>(get_color_channel(channel, color1)) -
                    static_cast<int>(get_color_channel(channel, color2)));
  };
  for (const auto& expected : param.colors) {
    const SkBitmap& bitmap = frame->Bitmap();
    SkColor frame_color =
        bitmap.getColor(expected.point.x(), expected.point.y());

    EXPECT_LE(color_difference(SkColorChannel::kR, frame_color, expected.color),
              param.color_threshold);
    EXPECT_LE(color_difference(SkColorChannel::kG, frame_color, expected.color),
              param.color_threshold);
    EXPECT_LE(color_difference(SkColorChannel::kB, frame_color, expected.color),
              param.color_threshold);
    // TODO(ryoh): Create alpha_threshold field for alpha channels.
    EXPECT_LE(color_difference(SkColorChannel::kA, frame_color, expected.color),
              param.color_threshold);
    if (param.color_type == ColorType::kMono ||
        param.color_type == ColorType::kMonoA) {
      EXPECT_EQ(SkColorGetR(frame_color), SkColorGetG(frame_color));
      EXPECT_EQ(SkColorGetR(frame_color), SkColorGetB(frame_color));
    }
  }
}

}  // namespace blink
