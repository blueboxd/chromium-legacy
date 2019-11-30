// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_decoder.h"

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/test/test_utils.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace {

std::vector<uint8_t> GetValidPngData() {
  // 1x1 PNG. Does not get much smaller than this.
  static const char kPngData[] =
      "\x89\x50\x4e\x47\x0d\x0a\x1a\x0a\x00\x00\x00\x0d\x49\x48\x44\x52"
      "\x00\x00\x00\x01\x00\x00\x00\x01\x08\x02\x00\x00\x00\x90\x77\x53"
      "\xde\x00\x00\x00\x0c\x49\x44\x41\x54\x08\xd7\x63\xf8\xff\xff\x3f"
      "\x00\x05\xfe\x02\xfe\xdc\xcc\x59\xe7\x00\x00\x00\x00\x49\x45\x4e"
      "\x44\xae\x42\x60\x82";
  // Need to specify the buffer size because it contains NULs.
  return std::vector<uint8_t>(kPngData, kPngData + sizeof(kPngData) - 1);
}

#if defined(OS_CHROMEOS)

std::vector<uint8_t> GetValidJpgData() {
  // 1x1 JPG created from the 1x1 PNG above.
  static const char kJpgData[] =
      "\xFF\xD8\xFF\xE0\x00\x10\x4A\x46\x49\x46\x00\x01\x01\x01\x00\x48"
      "\x00\x48\x00\x00\xFF\xDB\x00\x43\x00\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xDB\x00\x43\x01\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF"
      "\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xFF\xC0"
      "\x00\x11\x08\x00\x01\x00\x01\x03\x01\x22\x00\x02\x11\x01\x03\x11"
      "\x01\xFF\xC4\x00\x15\x00\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x03\xFF\xC4\x00\x14\x10\x01\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xC4"
      "\x00\x14\x01\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\xFF\xC4\x00\x14\x11\x01\x00\x00\x00\x00\x00\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xDA\x00\x0C\x03\x01"
      "\x00\x02\x11\x03\x11\x00\x3F\x00\xA0\x00\xFF\xD9";
  // Need to specify the buffer size because it contains NULs.
  return std::vector<uint8_t>(kJpgData, kJpgData + sizeof(kJpgData) - 1);
}

#endif  // defined(OS_CHROMEOS)

class TestImageRequest : public ImageDecoder::ImageRequest {
 public:
  explicit TestImageRequest(base::OnceClosure quit_closure)
      : decode_succeeded_(false),
        quit_closure_(std::move(quit_closure)),
        quit_called_(false) {}

  ~TestImageRequest() override {
    if (!quit_called_) {
      std::move(quit_closure_).Run();
    }
  }

  bool decode_succeeded() const { return decode_succeeded_; }

 private:
  void OnImageDecoded(const SkBitmap& decoded_image) override {
    decode_succeeded_ = true;
    Quit();
  }

  void OnDecodeImageFailed() override {
    Quit();
  }

  void Quit() {
    EXPECT_FALSE(quit_called_);
    quit_called_ = true;
    std::move(quit_closure_).Run();
  }

  bool decode_succeeded_;

  base::OnceClosure quit_closure_;
  bool quit_called_;

  DISALLOW_COPY_AND_ASSIGN(TestImageRequest);
};

}  // namespace

class ImageDecoderBrowserTest : public InProcessBrowserTest {
};

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, Basic) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  TestImageRequest test_request(runner->QuitClosure());
  ImageDecoder::Start(&test_request, std::vector<uint8_t>());
  runner->Run();
  EXPECT_FALSE(test_request.decode_succeeded());
}

#if defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, BasicDecodeWithOptionsString) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  TestImageRequest test_request(runner->QuitClosure());
  const std::vector<uint8_t> data = GetValidPngData();
  ImageDecoder::StartWithOptions(&test_request,
                                 std::string(data.begin(), data.end()),
                                 ImageDecoder::ROBUST_PNG_CODEC,
                                 /*shrink_to_fit=*/false);
  runner->Run();
  EXPECT_TRUE(test_request.decode_succeeded());
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, RobustJpegCodecWithJpegData) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  TestImageRequest test_request(runner->QuitClosure());
  ImageDecoder::StartWithOptions(
      &test_request, GetValidJpgData(), ImageDecoder::ROBUST_JPEG_CODEC,
      /*shrink_to_fit=*/false, /*desired_image_frame_size=*/gfx::Size());
  runner->Run();
  EXPECT_TRUE(test_request.decode_succeeded());
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, RobustJpegCodecWithPngData) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  TestImageRequest test_request(runner->QuitClosure());
  ImageDecoder::StartWithOptions(
      &test_request, GetValidPngData(), ImageDecoder::ROBUST_JPEG_CODEC,
      /*shrink_to_fit=*/false, /*desired_image_frame_size=*/gfx::Size());
  runner->Run();
  // Should fail with PNG data because only JPEG data is allowed.
  EXPECT_FALSE(test_request.decode_succeeded());
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, RobustPngCodecWithPngData) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  TestImageRequest test_request(runner->QuitClosure());
  ImageDecoder::StartWithOptions(
      &test_request, GetValidPngData(), ImageDecoder::ROBUST_PNG_CODEC,
      /*shrink_to_fit=*/false, /*desired_image_frame_size=*/gfx::Size());
  runner->Run();
  EXPECT_TRUE(test_request.decode_succeeded());
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, RobustPngCodecWithJpegData) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  TestImageRequest test_request(runner->QuitClosure());
  ImageDecoder::StartWithOptions(
      &test_request, GetValidJpgData(), ImageDecoder::ROBUST_PNG_CODEC,
      /*shrink_to_fit=*/false, /*desired_image_frame_size=*/gfx::Size());
  runner->Run();
  // Should fail with JPEG data because only PNG data is allowed.
  EXPECT_FALSE(test_request.decode_succeeded());
}

#endif  // defined(OS_CHROMEOS)

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, BasicDecode) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  TestImageRequest test_request(runner->QuitClosure());
  ImageDecoder::Start(&test_request, GetValidPngData());
  runner->Run();
  EXPECT_TRUE(test_request.decode_succeeded());
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, BasicDecodeString) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  TestImageRequest test_request(runner->QuitClosure());
  const std::vector<uint8_t> data = GetValidPngData();
  ImageDecoder::Start(&test_request, std::string(data.begin(), data.end()));
  runner->Run();
  EXPECT_TRUE(test_request.decode_succeeded());
}

IN_PROC_BROWSER_TEST_F(ImageDecoderBrowserTest, StartAndDestroy) {
  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  std::unique_ptr<TestImageRequest> test_request(
      new TestImageRequest(runner->QuitClosure()));
  ImageDecoder::Start(test_request.get(), std::vector<uint8_t>());
  test_request.reset();
  runner->Run();
}
