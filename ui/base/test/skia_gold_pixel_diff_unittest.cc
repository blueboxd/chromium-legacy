// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/test/skia_gold_pixel_diff.h"

#include "base/command_line.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/native_widget_types.h"

using ::testing::_;

class MockSkiaGoldPixelDiff : public SkiaGoldPixelDiff {
 public:
  MockSkiaGoldPixelDiff() = default;
  MOCK_CONST_METHOD1(LaunchProcess, int(const base::CommandLine&));
  bool UploadToSkiaGoldServer(
      const base::FilePath& local_file_path,
      const std::string& remote_golden_image_name) const override {
    return true;
  }
};

class SkiaGoldPixelDiffTest : public ::testing::Test {
 public:
  SkiaGoldPixelDiffTest() {
    auto* cmd_line = base::CommandLine::ForCurrentProcess();
    cmd_line->AppendSwitchASCII("git-revision", "test");
  }

  ~SkiaGoldPixelDiffTest() override {}

 protected:
  DISALLOW_COPY_AND_ASSIGN(SkiaGoldPixelDiffTest);
};

TEST_F(SkiaGoldPixelDiffTest, CompareScreenshotBySkBitmap) {
  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);
  MockSkiaGoldPixelDiff mock_pixel;
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", bitmap);
  EXPECT_TRUE(ret);
}

TEST_F(SkiaGoldPixelDiffTest, BypassSkiaGoldFunctionality) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      "bypass-skia-gold-functionality");

  SkBitmap bitmap;
  SkImageInfo info =
      SkImageInfo::Make(10, 10, SkColorType::kBGRA_8888_SkColorType,
                        SkAlphaType::kPremul_SkAlphaType);
  bitmap.allocPixels(info, 10 * 4);
  MockSkiaGoldPixelDiff mock_pixel;
  EXPECT_CALL(mock_pixel, LaunchProcess(_)).Times(0);
  mock_pixel.Init("Prefix");
  bool ret = mock_pixel.CompareScreenshot("test", bitmap);
  EXPECT_TRUE(ret);
}
