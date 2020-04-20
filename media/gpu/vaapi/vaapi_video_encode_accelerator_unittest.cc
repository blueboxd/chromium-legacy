// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/vaapi/vaapi_video_encode_accelerator.h"

#include "base/test/task_environment.h"
#include "media/video/video_encode_accelerator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

constexpr gfx::Size kDefaultEncodeSize(1280, 720);
constexpr uint32_t kDefaultBitrateBps = 4 * 1000 * 1000;
constexpr uint32_t kDefaultFramerate = 30;
const VideoEncodeAccelerator::Config kDefaultVEAConfig(PIXEL_FORMAT_I420,
                                                       kDefaultEncodeSize,
                                                       VP8PROFILE_ANY,
                                                       kDefaultBitrateBps,
                                                       kDefaultFramerate);

class MockVideoEncodeAcceleratorClient : public VideoEncodeAccelerator::Client {
 public:
  MockVideoEncodeAcceleratorClient() = default;
  virtual ~MockVideoEncodeAcceleratorClient() = default;

  MOCK_METHOD3(RequireBitstreamBuffers,
               void(unsigned int, const gfx::Size&, size_t output_buffer_size));
  MOCK_METHOD2(BitstreamBufferReady,
               void(int32_t, const BitstreamBufferMetadata&));
  MOCK_METHOD1(NotifyError, void(VideoEncodeAccelerator::Error));
  MOCK_METHOD1(NotifyEncoderInfoChange, void(const VideoEncoderInfo& info));
};

struct VaapiVEAInitializeTestParam {
  uint8_t num_of_temporal_layers = 0;
  uint8_t num_of_spatial_layers = 0;
  bool expected_result;
};

class VaapiVEAInitializeTest
    : public ::testing::TestWithParam<VaapiVEAInitializeTestParam> {
 protected:
  VaapiVEAInitializeTest() = default;
  ~VaapiVEAInitializeTest() override = default;
  base::test::TaskEnvironment task_environment_;
};

TEST_P(VaapiVEAInitializeTest, SpatialLayerAndTemporalLayerEncoding) {
  VideoEncodeAccelerator::Config config = kDefaultVEAConfig;
  const uint8_t num_of_temporal_layers = GetParam().num_of_temporal_layers;
  const uint8_t num_of_spatial_layers = GetParam().num_of_spatial_layers;
  constexpr int kDenom[] = {4, 2, 1};
  for (uint8_t i = 0; i < num_of_spatial_layers; ++i) {
    VideoEncodeAccelerator::Config::SpatialLayer spatial_layer;
    int denom = kDenom[i];
    spatial_layer.width = kDefaultEncodeSize.width() / denom;
    spatial_layer.height = kDefaultEncodeSize.height() / denom;
    spatial_layer.bitrate_bps = kDefaultBitrateBps / denom;
    spatial_layer.framerate = kDefaultFramerate;
    spatial_layer.max_qp = 30;
    spatial_layer.num_of_temporal_layers = num_of_temporal_layers;
    config.spatial_layers.push_back(spatial_layer);
  }

  VaapiVideoEncodeAccelerator vea;
  MockVideoEncodeAcceleratorClient client;
  EXPECT_EQ(vea.Initialize(config, &client), GetParam().expected_result);
}

constexpr VaapiVEAInitializeTestParam kTestCases[] = {
    {1u, 3u, false},  // Spatial Layer only.
    {3u, 3u, false},  // Temporal + Spatial Layer.
};

INSTANTIATE_TEST_SUITE_P(SpatialLayerAndTemporalLayerEncoding,
                         VaapiVEAInitializeTest,
                         ::testing::ValuesIn(kTestCases));
}  // namespace
}  // namespace media
