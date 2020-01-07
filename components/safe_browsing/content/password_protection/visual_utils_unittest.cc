// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/password_protection/visual_utils.h"

#include "base/test/test_discardable_memory_allocator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace visual_utils {

namespace {

// Pixel value constants, all in BGR order.
const unsigned int kWhite = 0xffffffff;
const unsigned int kBlack = 0xff000000;
const unsigned int kRed = 0xff0000ff;
const unsigned int kGreen = 0xff00ff00;
const unsigned int kBlue = 0xffff0000;

}  // namespace

using ::testing::FloatEq;

class VisualUtilsTest : public testing::Test {
 protected:
  void SetUp() override {
    base::DiscardableMemoryAllocator::SetInstance(&test_allocator_);

    sk_sp<SkColorSpace> rec2020 = SkColorSpace::MakeRGB(
        {2.22222f, 0.909672f, 0.0903276f, 0.222222f, 0.0812429f, 0, 0},
        SkNamedGamut::kRec2020);
    SkImageInfo bitmap_info =
        SkImageInfo::Make(1000, 1000, SkColorType::kRGBA_8888_SkColorType,
                          SkAlphaType::kUnpremul_SkAlphaType, rec2020);

    ASSERT_TRUE(bitmap_.tryAllocPixels(bitmap_info));
  }

  void TearDown() override {
    base::DiscardableMemoryAllocator::SetInstance(nullptr);
  }

  // A test bitmap to work with. Initialized to be 1000x1000 in the Rec 2020
  // color space.
  SkBitmap bitmap_;

 private:
  // A DiscardableMemoryAllocator is needed for certain Skia operations.
  base::TestDiscardableMemoryAllocator test_allocator_;
};

TEST_F(VisualUtilsTest, TestSkColorToQuantizedColor) {
  // Test quantization
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 31)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 32)), 1u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 31, 0)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 32, 0)), 8u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(31, 0, 0)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(32, 0, 0)), 64u);

  // Test composition of RGB quantized values
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 0)), 0u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 0, 255)), 7u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(0, 255, 255)), 63u);
  EXPECT_EQ(SkColorToQuantizedColor(SkColorSetRGB(255, 255, 255)), 511u);
}

TEST_F(VisualUtilsTest, GetQuantizedR) {
  EXPECT_EQ(GetQuantizedR(0), 0);
  EXPECT_EQ(GetQuantizedR(64), 1);
  EXPECT_EQ(GetQuantizedR(448), 7);
}

TEST_F(VisualUtilsTest, GetQuantizedG) {
  EXPECT_EQ(GetQuantizedG(0), 0);
  EXPECT_EQ(GetQuantizedG(8), 1);
  EXPECT_EQ(GetQuantizedG(56), 7);
}

TEST_F(VisualUtilsTest, GetQuantizedB) {
  EXPECT_EQ(GetQuantizedB(0), 0);
  EXPECT_EQ(GetQuantizedB(1), 1);
  EXPECT_EQ(GetQuantizedB(7), 7);
}

TEST_F(VisualUtilsTest, GetHistogramForImageWhite) {
  VisualFeatures::ColorHistogram histogram;
  SkBitmap bitmap;

  // Draw white over half the image
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kWhite;

  ASSERT_TRUE(GetHistogramForImage(bitmap_, &histogram));
  ASSERT_EQ(histogram.bins_size(), 1);
  EXPECT_THAT(histogram.bins(0).centroid_x(),
              FloatEq(0.4995));  // All pixels are the same color, so centroid_x
                                 // is (0+1+...+999)/1000/1000 = 0.4995
  EXPECT_THAT(histogram.bins(0).centroid_y(), FloatEq(0.4995));
  EXPECT_EQ(histogram.bins(0).quantized_r(), 7);
  EXPECT_EQ(histogram.bins(0).quantized_g(), 7);
  EXPECT_EQ(histogram.bins(0).quantized_b(), 7);
  EXPECT_THAT(histogram.bins(0).weight(), FloatEq(1.0));
}

TEST_F(VisualUtilsTest, GetHistogramForImageHalfWhiteHalfBlack) {
  VisualFeatures::ColorHistogram histogram;

  // Draw white over half the image
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 500; y++)
      *bitmap_.getAddr32(x, y) = kWhite;

  // Draw black over half the image.
  for (int x = 0; x < 1000; x++)
    for (int y = 500; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kBlack;

  ASSERT_TRUE(GetHistogramForImage(bitmap_, &histogram));
  ASSERT_EQ(histogram.bins_size(), 2);

  EXPECT_THAT(histogram.bins(0).centroid_x(), FloatEq(0.4995));
  EXPECT_THAT(histogram.bins(0).centroid_y(), FloatEq(0.7495));
  EXPECT_EQ(histogram.bins(0).quantized_r(), 0);
  EXPECT_EQ(histogram.bins(0).quantized_g(), 0);
  EXPECT_EQ(histogram.bins(0).quantized_b(), 0);
  EXPECT_THAT(histogram.bins(0).weight(), FloatEq(0.5));

  EXPECT_THAT(histogram.bins(1).centroid_x(), FloatEq(0.4995));
  EXPECT_THAT(histogram.bins(1).centroid_y(), FloatEq(0.2495));
  EXPECT_EQ(histogram.bins(1).quantized_r(), 7);
  EXPECT_EQ(histogram.bins(1).quantized_g(), 7);
  EXPECT_EQ(histogram.bins(1).quantized_b(), 7);
  EXPECT_THAT(histogram.bins(1).weight(), FloatEq(0.5));
}

TEST_F(VisualUtilsTest, BlurImageWhite) {
  VisualFeatures::BlurredImage blurred;

  // Draw white over the image
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kWhite;

  ASSERT_TRUE(GetBlurredImage(bitmap_, &blurred));
  ASSERT_EQ(48, blurred.width());
  ASSERT_EQ(48, blurred.height());
  ASSERT_EQ(3u * 48u * 48u, blurred.data().size());
  for (size_t i = 0; i < 48u * 48u; i++) {
    EXPECT_EQ('\xff', blurred.data()[3 * i]);
    EXPECT_EQ('\xff', blurred.data()[3 * i + 1]);
    EXPECT_EQ('\xff', blurred.data()[3 * i + 2]);
  }
}

TEST_F(VisualUtilsTest, BlurImageRed) {
  VisualFeatures::BlurredImage blurred;

  // Draw red over the image.
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kRed;

  ASSERT_TRUE(GetBlurredImage(bitmap_, &blurred));
  ASSERT_EQ(48, blurred.width());
  ASSERT_EQ(48, blurred.height());
  ASSERT_EQ(3u * 48u * 48u, blurred.data().size());
  for (size_t i = 0; i < 48u * 48u; i++) {
    EXPECT_EQ('\xff', blurred.data()[3 * i]);
    EXPECT_EQ('\x00', blurred.data()[3 * i + 1]);
    EXPECT_EQ('\x00', blurred.data()[3 * i + 2]);
  }
}

TEST_F(VisualUtilsTest, BlurImageHalfWhiteHalfBlack) {
  VisualFeatures::BlurredImage blurred;

  // Draw black over half the image.
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 500; y++)
      *bitmap_.getAddr32(x, y) = kBlack;

  // Draw white over half the image
  for (int x = 0; x < 1000; x++)
    for (int y = 500; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kWhite;

  ASSERT_TRUE(GetBlurredImage(bitmap_, &blurred));
  ASSERT_EQ(48, blurred.width());
  ASSERT_EQ(48, blurred.height());
  ASSERT_EQ(3u * 48u * 48u, blurred.data().size());
  // The middle blocks may have been blurred to something between white and
  // black, so only verify the first 22 and last 22 rows.
  for (size_t i = 0; i < 22u * 48u; i++) {
    EXPECT_EQ('\x00', blurred.data()[3 * i]);
    EXPECT_EQ('\x00', blurred.data()[3 * i + 1]);
    EXPECT_EQ('\x00', blurred.data()[3 * i + 2]);
  }

  for (size_t i = 26u * 48u; i < 48u * 48u; i++) {
    EXPECT_EQ('\xff', blurred.data()[3 * i]);
    EXPECT_EQ('\xff', blurred.data()[3 * i + 1]);
    EXPECT_EQ('\xff', blurred.data()[3 * i + 2]);
  }
}

TEST_F(VisualUtilsTest, BlockMeanAverageOneBlock) {
  // Draw black over half the image.
  for (int x = 0; x < 1000; x++)
    for (int y = 0; y < 500; y++)
      *bitmap_.getAddr32(x, y) = kBlack;

  // Draw white over half the image
  for (int x = 0; x < 1000; x++)
    for (int y = 500; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kWhite;

  std::unique_ptr<SkBitmap> blocks = BlockMeanAverage(bitmap_, 1000);
  ASSERT_EQ(1, blocks->width());
  ASSERT_EQ(1, blocks->height());
  EXPECT_EQ(blocks->getColor(0, 0), SkColorSetRGB(127, 127, 127));
}

TEST_F(VisualUtilsTest, BlockMeanAveragePartialBlocks) {
  // Draw a white, red, green, and blue box with the expected block sizes.
  for (int x = 0; x < 600; x++)
    for (int y = 0; y < 600; y++)
      *bitmap_.getAddr32(x, y) = kWhite;

  for (int x = 600; x < 1000; x++)
    for (int y = 0; y < 600; y++)
      *bitmap_.getAddr32(x, y) = kRed;

  for (int x = 0; x < 600; x++)
    for (int y = 600; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kGreen;

  for (int x = 600; x < 1000; x++)
    for (int y = 600; y < 1000; y++)
      *bitmap_.getAddr32(x, y) = kBlue;

  std::unique_ptr<SkBitmap> blocks = BlockMeanAverage(bitmap_, 600);
  ASSERT_EQ(2, blocks->width());
  ASSERT_EQ(2, blocks->height());
  EXPECT_EQ(*blocks->getAddr32(0, 0), kWhite);
  EXPECT_EQ(*blocks->getAddr32(1, 0), kRed);
  EXPECT_EQ(*blocks->getAddr32(0, 1), kGreen);
  EXPECT_EQ(*blocks->getAddr32(1, 1), kBlue);
}

}  // namespace visual_utils
}  // namespace safe_browsing
