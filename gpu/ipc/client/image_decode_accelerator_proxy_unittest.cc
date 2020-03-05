// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "cc/paint/paint_image.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/client/image_decode_accelerator_proxy.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_space.h"

using ::testing::DeleteArg;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::StrictMock;

namespace gpu {

namespace {
constexpr int kChannelId = 5;
constexpr int32_t kRasterCmdBufferRouteId = 3;
constexpr gfx::Size kOutputSize(2, 2);

MATCHER_P(IpcMessageEqualTo, expected, "") {
  // Get params from actual IPC message.
  GpuChannelMsg_ScheduleImageDecode::Param actual_param_tuple;
  if (!GpuChannelMsg_ScheduleImageDecode::Read(arg, &actual_param_tuple))
    return false;

  GpuChannelMsg_ScheduleImageDecode_Params params =
      std::get<0>(actual_param_tuple);
  const uint64_t release_count = std::get<1>(actual_param_tuple);

  // Get params from expected IPC Message.
  GpuChannelMsg_ScheduleImageDecode::Param expected_param_tuple;
  if (!GpuChannelMsg_ScheduleImageDecode::Read(expected, &expected_param_tuple))
    return false;

  GpuChannelMsg_ScheduleImageDecode_Params expected_params =
      std::get<0>(expected_param_tuple);
  const uint64_t expected_release_count = std::get<1>(expected_param_tuple);

  // Compare all relevant fields.
  return arg->routing_id() == expected->routing_id() &&
         release_count == expected_release_count &&
         params.encoded_data == expected_params.encoded_data &&
         params.output_size == expected_params.output_size &&
         params.raster_decoder_route_id ==
             expected_params.raster_decoder_route_id &&
         params.transfer_cache_entry_id ==
             expected_params.transfer_cache_entry_id &&
         params.discardable_handle_shm_id ==
             expected_params.discardable_handle_shm_id &&
         params.discardable_handle_shm_offset ==
             expected_params.discardable_handle_shm_offset &&
         params.discardable_handle_release_count ==
             expected_params.discardable_handle_release_count &&
         params.target_color_space == expected_params.target_color_space &&
         params.needs_mips == expected_params.needs_mips;
}

}  // namespace

class MockGpuChannelHost : public GpuChannelHost {
 public:
  MockGpuChannelHost() : MockGpuChannelHost(GPUInfo()) {}

  MockGpuChannelHost(const GPUInfo& info)
      : GpuChannelHost(kChannelId,
                       info,
                       GpuFeatureInfo(),
                       mojo::ScopedMessagePipeHandle(mojo::MessagePipeHandle(
                           mojo::kInvalidHandleValue))) {}

  MOCK_METHOD1(Send, bool(IPC::Message*));

 protected:
  ~MockGpuChannelHost() override {}
};

class ImageDecodeAcceleratorProxyTest : public ::testing::Test {
 public:
  ImageDecodeAcceleratorProxyTest()
      : gpu_channel_host_(
            base::MakeRefCounted<StrictMock<MockGpuChannelHost>>()),
        proxy_(gpu_channel_host_.get(),
               static_cast<int32_t>(
                   GpuChannelReservedRoutes::kImageDecodeAccelerator)) {}

  ~ImageDecodeAcceleratorProxyTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  scoped_refptr<StrictMock<MockGpuChannelHost>> gpu_channel_host_;
  ImageDecodeAcceleratorProxy proxy_;
};

TEST_F(ImageDecodeAcceleratorProxyTest, ScheduleImageDecodeSendsMessage) {
  const uint8_t image[4] = {1, 2, 3, 4};
  base::span<const uint8_t> encoded_data =
      base::span<const uint8_t>(image, base::size(image));

  const gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

  GpuChannelMsg_ScheduleImageDecode_Params expected_params;
  expected_params.encoded_data.assign(encoded_data.begin(), encoded_data.end());
  expected_params.output_size = kOutputSize;
  expected_params.raster_decoder_route_id = kRasterCmdBufferRouteId;
  expected_params.transfer_cache_entry_id = 1u;
  expected_params.discardable_handle_shm_id = 2;
  expected_params.discardable_handle_shm_offset = 3u;
  expected_params.discardable_handle_release_count = 4u;
  expected_params.target_color_space = color_space;
  expected_params.needs_mips = false;

  GpuChannelMsg_ScheduleImageDecode expected_message(
      static_cast<int32_t>(GpuChannelReservedRoutes::kImageDecodeAccelerator),
      std::move(expected_params), /*release_count=*/1u);

  {
    EXPECT_CALL(*gpu_channel_host_, Send(IpcMessageEqualTo(&expected_message)))
        .Times(1)
        .WillOnce(DoAll(DeleteArg<0>(),
                        Return(false)));  // Delete object passed to Send.
  }

  SyncToken token = proxy_.ScheduleImageDecode(
      encoded_data, kOutputSize,
      CommandBufferIdFromChannelAndRoute(kChannelId, kRasterCmdBufferRouteId),
      /*transfer_cache_entry_id=*/1u,
      /*discardable_handle_shm_id=*/2,
      /*discardable_handle_shm_offset=*/3u,
      /*discardable_handle_release_count=*/4u, color_space,
      /*needs_mips=*/false);

  task_environment_.RunUntilIdle();
  testing::Mock::VerifyAndClearExpectations(gpu_channel_host_.get());

  EXPECT_EQ(ChannelIdFromCommandBufferId(token.command_buffer_id()),
            kChannelId);
  EXPECT_EQ(
      RouteIdFromCommandBufferId(token.command_buffer_id()),
      static_cast<int32_t>(GpuChannelReservedRoutes::kImageDecodeAccelerator));
  EXPECT_EQ(token.release_count(), 1u);
}

class ImageDecodeAcceleratorProxySubsamplingTest
    : public testing::TestWithParam<cc::YUVSubsampling> {
 public:
  ImageDecodeAcceleratorProxySubsamplingTest() = default;
  ~ImageDecodeAcceleratorProxySubsamplingTest() override = default;

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_P(ImageDecodeAcceleratorProxySubsamplingTest, JPEGSubsamplingIsSupported) {
  cc::ImageHeaderMetadata image_metadata;
  image_metadata.yuv_subsampling = GetParam();
  image_metadata.image_type = cc::ImageType::kJPEG;
  image_metadata.all_data_received_prior_to_decode = true;
  image_metadata.has_embedded_color_profile = false;
  image_metadata.image_size = gfx::Size(100, 200);
  image_metadata.jpeg_is_progressive = false;

  ImageDecodeAcceleratorSupportedProfile profile;
  profile.image_type = ImageDecodeAcceleratorType::kJpeg;
  profile.min_encoded_dimensions = gfx::Size(0, 0);
  profile.max_encoded_dimensions = gfx::Size(1920, 1080);

  static_assert(
      // TODO(andrescj): refactor to instead have a static_assert at the
      // declaration site of ImageDecodeAcceleratorSubsampling to make sure it
      // has the same number of entries as cc::YUVSubsampling.
      static_cast<int>(ImageDecodeAcceleratorSubsampling::kMaxValue) == 2,
      "ImageDecodeAcceleratorProxySubsamplingTest.JPEGSubsamplingIsSupported "
      "must be adapted to support all subsampling factors in "
      "ImageDecodeAcceleratorSubsampling");
  switch (GetParam()) {
    case cc::YUVSubsampling::k420:
      profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k420);
      break;
    case cc::YUVSubsampling::k422:
      profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k422);
      break;
    case cc::YUVSubsampling::k444:
      profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k444);
      break;
    default:
      return;
  }
  GPUInfo gpu_info;
  gpu_info.image_decode_accelerator_supported_profiles.push_back(profile);

  auto gpu_channel_host(
      base::MakeRefCounted<StrictMock<MockGpuChannelHost>>(gpu_info));
  ImageDecodeAcceleratorProxy proxy(
      gpu_channel_host.get(),
      static_cast<int32_t>(GpuChannelReservedRoutes::kImageDecodeAccelerator));

  EXPECT_TRUE(proxy.IsImageSupported(&image_metadata));
}

TEST_P(ImageDecodeAcceleratorProxySubsamplingTest,
       JPEGSubsamplingIsNotSupported) {
  cc::ImageHeaderMetadata image_metadata;
  image_metadata.yuv_subsampling = GetParam();
  image_metadata.image_type = cc::ImageType::kJPEG;
  image_metadata.all_data_received_prior_to_decode = true;
  image_metadata.has_embedded_color_profile = false;
  image_metadata.image_size = gfx::Size(100, 200);
  image_metadata.jpeg_is_progressive = false;

  ImageDecodeAcceleratorSupportedProfile profile;
  profile.image_type = ImageDecodeAcceleratorType::kJpeg;
  profile.min_encoded_dimensions = gfx::Size(0, 0);
  profile.max_encoded_dimensions = gfx::Size(1920, 1080);

  static_assert(
      // TODO(andrescj): refactor to instead have a static_assert at the
      // declaration site of ImageDecodeAcceleratorSubsampling to make sure it
      // has the same number of entries as cc::YUVSubsampling.
      static_cast<int>(ImageDecodeAcceleratorSubsampling::kMaxValue) == 2,
      "ImageDecodeAcceleratorProxySubsamplingTest.JPEGSubsamplingIsNotSupported"
      "must be adapted to support all subsampling factors in "
      "ImageDecodeAcceleratorSubsampling");

  // Advertise support for all subsamplings except the GetParam() one.
  if (GetParam() != cc::YUVSubsampling::k420)
    profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k420);
  if (GetParam() != cc::YUVSubsampling::k422)
    profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k422);
  if (GetParam() != cc::YUVSubsampling::k444)
    profile.subsamplings.push_back(ImageDecodeAcceleratorSubsampling::k444);

  GPUInfo gpu_info;
  gpu_info.image_decode_accelerator_supported_profiles.push_back(profile);

  auto gpu_channel_host(
      base::MakeRefCounted<StrictMock<MockGpuChannelHost>>(gpu_info));
  ImageDecodeAcceleratorProxy proxy(
      gpu_channel_host.get(),
      static_cast<int32_t>(GpuChannelReservedRoutes::kImageDecodeAccelerator));

  EXPECT_FALSE(proxy.IsImageSupported(&image_metadata));
}

INSTANTIATE_TEST_SUITE_P(ImageDecodeAcceleratorProxySubsample,
                         ImageDecodeAcceleratorProxySubsamplingTest,
                         testing::Values(cc::YUVSubsampling::k410,
                                         cc::YUVSubsampling::k411,
                                         cc::YUVSubsampling::k420,
                                         cc::YUVSubsampling::k422,
                                         cc::YUVSubsampling::k440,
                                         cc::YUVSubsampling::k444,
                                         cc::YUVSubsampling::kUnknown));

}  // namespace gpu
