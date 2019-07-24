// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <va/va.h>

#include <memory>
#include <string>

// This has to be included first.
// See http://code.google.com/p/googletest/issues/detail?id=371
#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/test_data_util.h"
#include "media/capture/video/chromeos/local_gpu_memory_buffer_manager.h"
#include "media/gpu/vaapi/test_utils.h"
#include "media/gpu/vaapi/vaapi_image_decoder.h"
#include "media/gpu/vaapi/vaapi_utils.h"
#include "media/gpu/vaapi/vaapi_webp_decoder.h"
#include "media/gpu/vaapi/vaapi_wrapper.h"
#include "media/parsers/vp8_parser.h"
#include "media/parsers/webp_parser.h"
#include "third_party/libwebp/src/webp/decode.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap_handle.h"

namespace media {
namespace {

constexpr const char* kSmallImageFilename =
    "RGB_noise_large_pixels_115x115.webp";
constexpr const char* kMediumImageFilename =
    "RGB_noise_large_pixels_2015x2015.webp";
constexpr const char* kLargeImageFilename =
    "RGB_noise_large_pixels_4000x4000.webp";

constexpr const char* kLowFreqImageFilename = "solid_green_2015x2015.webp";
constexpr const char* kMedFreqImageFilename =
    "BlackAndWhite_criss-cross_pattern_2015x2015.webp";
constexpr const char* kHighFreqImageFilename = "RGB_noise_2015x2015.webp";

const vaapi_test_utils::TestParam kTestCases[] = {
    {"SmallImage_115x115", kSmallImageFilename},
    {"MediumImage_2015x2015", kMediumImageFilename},
    {"LargeImage_4000x4000", kLargeImageFilename},
    {"LowFreqImage", kLowFreqImageFilename},
    {"MedFreqImage", kMedFreqImageFilename},
    {"HighFreqImage", kHighFreqImageFilename},
};

// Any number above 99.5% should do. We are being very aggressive here.
constexpr double kMinSsim = 0.999;

// WebpDecode*() returns memory that has to be released in a specific way.
struct WebpDecodeDeleter {
  void operator()(void* ptr) { WebPFree(ptr); }
};

}  // namespace

class VaapiWebPDecoderTest
    : public testing::TestWithParam<vaapi_test_utils::TestParam> {
 protected:
  VaapiWebPDecoderTest() {
    const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
    if (cmd_line && cmd_line->HasSwitch("test_data_path"))
      test_data_path_ = cmd_line->GetSwitchValueASCII("test_data_path");
  }

  void SetUp() override {
    if (!VaapiWrapper::IsDecodeSupported(VAProfileVP8Version0_3)) {
      DLOG(INFO) << "VP8 decoding is not supported by the VA-API.";
      GTEST_SKIP();
    }

    ASSERT_TRUE(decoder_.Initialize(base::BindRepeating(
        []() { LOG(FATAL) << "Oh noes! Decoder failed"; })));
  }

  // Find the location of the specified test file. If a file with specified path
  // is not found, treat the file as being relative to the test file directory.
  // This is either a custom test data path provided by --test_data_path, or the
  // default test data path (//media/test/data).
  base::FilePath FindTestDataFilePath(const std::string& file_name) {
    const base::FilePath file_path = base::FilePath(file_name);
    if (base::PathExists(file_path))
      return file_path;
    if (!test_data_path_.empty())
      return base::FilePath(test_data_path_).Append(file_path);
    return GetTestDataFilePath(file_name);
  }

  scoped_refptr<gfx::NativePixmapDmaBuf> DecodeToNativePixmapDmaBuf(
      base::span<const uint8_t> encoded_image,
      VaapiImageDecodeStatus* status = nullptr) {
    const VaapiImageDecodeStatus decode_status = decoder_.Decode(encoded_image);
    EXPECT_EQ(!!decoder_.GetScopedVASurface(),
              decode_status == VaapiImageDecodeStatus::kSuccess);

    // Still try to get the pixmap when decode fails.
    VaapiImageDecodeStatus pixmap_status;
    scoped_refptr<gfx::NativePixmapDmaBuf> pixmap =
        decoder_.ExportAsNativePixmapDmaBuf(&pixmap_status);
    EXPECT_EQ(!!pixmap, pixmap_status == VaapiImageDecodeStatus::kSuccess);

    // Return the first fail status.
    if (status) {
      *status = decode_status != VaapiImageDecodeStatus::kSuccess
                    ? decode_status
                    : pixmap_status;
    }
    return pixmap;
  }

 protected:
  std::string test_data_path_;
  VaapiWebPDecoder decoder_;
};

TEST_P(VaapiWebPDecoderTest, DecodeAndExportAsNativePixmapDmaBuf) {
  base::FilePath input_file = FindTestDataFilePath(GetParam().filename);
  std::string webp_data;
  ASSERT_TRUE(base::ReadFileToString(input_file, &webp_data))
      << "failed to read input data from " << input_file.value();
  const auto encoded_image = base::make_span<const uint8_t>(
      reinterpret_cast<const uint8_t*>(webp_data.data()), webp_data.size());

  // Decode the image using the VA-API and wrap the decoded image in a
  // DecodedImage object.
  ASSERT_TRUE(VaapiWrapper::IsDecodingSupportedForInternalFormat(
      VAProfileVP8Version0_3, VA_RT_FORMAT_YUV420));

  VaapiImageDecodeStatus status;
  scoped_refptr<gfx::NativePixmapDmaBuf> pixmap =
      DecodeToNativePixmapDmaBuf(encoded_image, &status);
  ASSERT_EQ(VaapiImageDecodeStatus::kSuccess, status);
  EXPECT_FALSE(decoder_.GetScopedVASurface());
  ASSERT_TRUE(pixmap);
  ASSERT_EQ(gfx::BufferFormat::YUV_420_BIPLANAR, pixmap->GetBufferFormat());

  gfx::NativePixmapHandle handle = pixmap->ExportHandle();
  LocalGpuMemoryBufferManager gpu_memory_buffer_manager;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer =
      gpu_memory_buffer_manager.ImportDmaBuf(handle, pixmap->GetBufferSize(),
                                             pixmap->GetBufferFormat());
  ASSERT_TRUE(gpu_memory_buffer);
  ASSERT_TRUE(gpu_memory_buffer->Map());
  ASSERT_EQ(gfx::BufferFormat::YUV_420_BIPLANAR,
            gpu_memory_buffer->GetFormat());

  vaapi_test_utils::DecodedImage hw_decoded_webp{};
  hw_decoded_webp.fourcc = VA_FOURCC_NV12;
  hw_decoded_webp.number_of_planes = 2u;
  hw_decoded_webp.size = gpu_memory_buffer->GetSize();
  for (size_t plane = 0u;
       plane < base::strict_cast<size_t>(hw_decoded_webp.number_of_planes);
       plane++) {
    hw_decoded_webp.planes[plane].data =
        static_cast<uint8_t*>(gpu_memory_buffer->memory(plane));
    hw_decoded_webp.planes[plane].stride = gpu_memory_buffer->stride(plane);
  }

  // Decode the image using libwebp and wrap the decoded image in a
  // DecodedImage object.
  std::unique_ptr<Vp8FrameHeader> parse_result = ParseWebPImage(encoded_image);
  ASSERT_TRUE(parse_result);

  int reference_width;
  int reference_height;
  int y_stride;
  int uv_stride;
  uint8_t* libwebp_u_plane = nullptr;
  uint8_t* libwebp_v_plane = nullptr;
  std::unique_ptr<uint8_t, WebpDecodeDeleter> libwebp_y_plane(
      WebPDecodeYUV(encoded_image.data(), encoded_image.size(),
                    &reference_width, &reference_height, &libwebp_u_plane,
                    &libwebp_v_plane, &y_stride, &uv_stride));
  ASSERT_TRUE(libwebp_y_plane && libwebp_u_plane && libwebp_v_plane);
  ASSERT_EQ(reference_width, base::strict_cast<int>(parse_result->width));
  ASSERT_EQ(reference_height, base::strict_cast<int>(parse_result->height));

  // Wrap the software decoded image in a DecodedImage object.
  vaapi_test_utils::DecodedImage sw_decoded_webp{};
  sw_decoded_webp.fourcc = VA_FOURCC_I420;
  sw_decoded_webp.number_of_planes = 3u;
  sw_decoded_webp.size = gfx::Size(reference_width, reference_height);
  sw_decoded_webp.planes[0].data = libwebp_y_plane.get();
  sw_decoded_webp.planes[0].stride = y_stride;
  sw_decoded_webp.planes[1].data = libwebp_u_plane;
  sw_decoded_webp.planes[1].stride = uv_stride;
  sw_decoded_webp.planes[2].data = libwebp_v_plane;
  sw_decoded_webp.planes[2].stride = uv_stride;

  EXPECT_TRUE(vaapi_test_utils::CompareImages(sw_decoded_webp, hw_decoded_webp,
                                              kMinSsim));
  gpu_memory_buffer->Unmap();
}

// TODO(crbug.com/986073): expand test coverage. See
// vaapi_jpeg_decoder_unittest.cc as reference:
// cs.chromium.org/chromium/src/media/gpu/vaapi/vaapi_jpeg_decoder_unittest.cc
INSTANTIATE_TEST_SUITE_P(,
                         VaapiWebPDecoderTest,
                         testing::ValuesIn(kTestCases),
                         vaapi_test_utils::TestParamToString);

}  // namespace media
