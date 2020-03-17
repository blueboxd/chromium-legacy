// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upboarding/query_tiles/internal/image_decoder.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace upboarding {
namespace {

const base::FilePath::CharType kTestDataDir[] =
    FILE_PATH_LITERAL("chrome/test/data/image_decoding/droids.jpg");

class ImageDecoderTest : public testing::Test {
 public:
  ImageDecoderTest() = default;
  ImageDecoderTest(const ImageDecoderTest&) = delete;
  ImageDecoderTest& operator=(const ImageDecoderTest&) = delete;
  ~ImageDecoderTest() override = default;

  void SetUp() override { image_decoder_ = ImageDecoder::Create(); }

 protected:
  void Decode(ImageDecoder::EncodedData data,
              const gfx::Size& size,
              ImageDecoder::DecodeCallback callback) {
    image_decoder_->Decode(std::move(data), size, std::move(callback));
  }

 private:
  // Needed test support objects.
  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder decoder_service;

  std::unique_ptr<ImageDecoder> image_decoder_;
};

// Verifies empty input will result in empty output.
TEST_F(ImageDecoderTest, DecodeEmpty) {
  base::RunLoop loop;
  auto callback = base::BindLambdaForTesting([&](const SkBitmap& bitmap) {
    EXPECT_TRUE(bitmap.empty());
    loop.Quit();
  });

  Decode(ImageDecoder::EncodedData(), gfx::Size(1, 1), std::move(callback));
  loop.Run();
}

// Decodes an image.
TEST_F(ImageDecoderTest, Decode) {
  // Read in a test image.
  base::FilePath data_dir;
  ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &data_dir));
  base::FilePath file_path = data_dir.Append(base::FilePath(kTestDataDir));
  std::string file_data;
  ASSERT_TRUE(base::ReadFileToString(file_path, &file_data));
  EXPECT_FALSE(file_data.empty());
  ImageDecoder::EncodedData data(file_data.begin(), file_data.end());
  EXPECT_FALSE(data.empty());

  // Decode the image data.
  base::RunLoop loop;
  auto callback = base::BindLambdaForTesting([&](const SkBitmap& bitmap) {
    EXPECT_FALSE(bitmap.empty());
    loop.Quit();
  });
  Decode(std::move(data), gfx::Size(16, 16), std::move(callback));
  loop.Run();
}

}  // namespace
}  // namespace upboarding
