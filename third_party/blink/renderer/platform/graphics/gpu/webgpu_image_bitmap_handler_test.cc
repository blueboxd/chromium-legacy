// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_image_bitmap_handler.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

namespace blink {

static constexpr uint64_t kMaxArrayLength = 40000;

class WebGPUImageBitmapHandlerTest : public testing::Test {
 protected:
  void SetUp() override {}

  void VerifyCopyBytesForWebGPU(uint64_t width,
                                uint64_t height,
                                SkImageInfo info,
                                CanvasColorParams param,
                                IntRect copyRect) {
    const uint64_t content_length = width * height * param.BytesPerPixel();
    std::array<uint8_t, kMaxArrayLength> contents = {0};
    // Initialize contents.
    for (size_t i = 0; i < content_length; ++i) {
      contents[i] = i % std::numeric_limits<uint8_t>::max();
    }

    sk_sp<SkData> image_pixels =
        SkData::MakeWithCopy(contents.data(), content_length);
    scoped_refptr<StaticBitmapImage> image =
        StaticBitmapImage::Create(std::move(image_pixels), info);

    WebGPUImageUploadSizeInfo wgpu_info =
        ComputeImageBitmapWebGPUUploadSizeInfo(copyRect, param);

    const uint64_t result_length = wgpu_info.size_in_bytes;
    std::array<uint8_t, kMaxArrayLength> results = {0};
    bool success = CopyBytesFromImageBitmapForWebGPU(
        image, base::span<uint8_t>(results.data(), result_length), copyRect,
        param);
    ASSERT_EQ(success, true);

    // Compare content and results
    uint32_t row_pitch = wgpu_info.wgpu_row_pitch;
    uint32_t content_row_index =
        (copyRect.Y() * width + copyRect.X()) * param.BytesPerPixel();
    uint32_t result_row_index = 0;
    for (int i = 0; i < copyRect.Height(); ++i) {
      EXPECT_EQ(0,
                memcmp(&contents[content_row_index], &results[result_row_index],
                       copyRect.Width() * param.BytesPerPixel()));
      content_row_index += width * param.BytesPerPixel();
      result_row_index += row_pitch;
    }
  }
};

// Test calculate size
TEST_F(WebGPUImageBitmapHandlerTest, VerifyGetWGPUResourceInfo) {
  uint64_t imageWidth = 63;
  uint64_t imageHeight = 1;
  CanvasColorParams param(CanvasColorSpace::kSRGB, CanvasPixelFormat::kRGBA8,
                          OpacityMode::kNonOpaque);

  // Prebaked expected values.
  uint32_t expected_row_pitch = 256;
  uint64_t expected_size = 256;

  IntRect test_rect(0, 0, imageWidth, imageHeight);
  WebGPUImageUploadSizeInfo info =
      ComputeImageBitmapWebGPUUploadSizeInfo(test_rect, param);
  ASSERT_EQ(expected_size, info.size_in_bytes);
  ASSERT_EQ(expected_row_pitch, info.wgpu_row_pitch);
}

// Copy full image bitmap test
TEST_F(WebGPUImageBitmapHandlerTest, VerifyCopyBytesFromImageBitmapForWebGPU) {
  uint64_t imageWidth = 4;
  uint64_t imageHeight = 2;
  SkImageInfo info = SkImageInfo::Make(
      imageWidth, imageHeight, SkColorType::kRGBA_8888_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType, SkColorSpace::MakeSRGB());

  IntRect image_data_rect(0, 0, imageWidth, imageHeight);
  CanvasColorParams color_params(CanvasColorSpace::kSRGB,
                                 CanvasPixelFormat::kRGBA8,
                                 OpacityMode::kNonOpaque);
  VerifyCopyBytesForWebGPU(imageWidth, imageHeight, info, color_params,
                           image_data_rect);
}

// Copy sub image bitmap test
TEST_F(WebGPUImageBitmapHandlerTest, VerifyCopyBytesFromSubImageBitmap) {
  uint64_t imageWidth = 63;
  uint64_t imageHeight = 4;
  SkImageInfo info = SkImageInfo::Make(
      imageWidth, imageHeight, SkColorType::kRGBA_8888_SkColorType,
      SkAlphaType::kUnpremul_SkAlphaType, SkColorSpace::MakeSRGB());

  IntRect image_data_rect(2, 2, 60, 2);
  CanvasColorParams color_params(CanvasColorSpace::kSRGB,
                                 CanvasPixelFormat::kRGBA8,
                                 OpacityMode::kNonOpaque);
  VerifyCopyBytesForWebGPU(imageWidth, imageHeight, info, color_params,
                           image_data_rect);
}

}  // namespace blink
