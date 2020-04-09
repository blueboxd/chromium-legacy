// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_encode_accelerator_mojom_traits.h"
#include "media/mojo/mojom/video_encoder_info_mojom_traits.h"

#include "media/video/video_encode_accelerator.h"
#include "media/video/video_encoder_info.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// These binary operators are implemented here because they are used in this
// unittest. They cannot be enclosed by anonymous namespace, because they must
// be visible by gtest in linking.
bool operator==(const ::media::ScalingSettings& l,
                const ::media::ScalingSettings& r) {
  return l.min_qp == r.min_qp && l.max_qp == r.max_qp;
}

bool operator!=(const ::media::ScalingSettings& l,
                const ::media::ScalingSettings& r) {
  return !(l == r);
}

bool operator==(const ::media::ResolutionBitrateLimit& l,
                const ::media::ResolutionBitrateLimit& r) {
  return (l.frame_size == r.frame_size &&
          l.min_start_bitrate_bps == r.min_start_bitrate_bps &&
          l.min_bitrate_bps == r.min_bitrate_bps &&
          l.max_bitrate_bps == r.max_bitrate_bps);
}

bool operator!=(const ::media::ResolutionBitrateLimit& l,
                const ::media::ResolutionBitrateLimit& r) {
  return !(l == r);
}

bool operator==(const ::media::VideoEncoderInfo& l,
                const ::media::VideoEncoderInfo& r) {
  if (l.implementation_name != r.implementation_name)
    return false;
  if (l.supports_native_handle != r.supports_native_handle)
    return false;
  if (l.has_trusted_rate_controller != r.has_trusted_rate_controller)
    return false;
  if (l.is_hardware_accelerated != r.is_hardware_accelerated)
    return false;
  if (l.supports_simulcast != r.supports_simulcast)
    return false;
  if (l.scaling_settings != r.scaling_settings)
    return false;
  for (size_t i = 0; i < ::media::VideoEncoderInfo::kMaxSpatialLayers; ++i) {
    if (l.fps_allocation[i] != r.fps_allocation[i])
      return false;
  }
  if (l.resolution_bitrate_limits != r.resolution_bitrate_limits)
    return false;
  return true;
}

bool operator==(
    const ::media::VideoEncodeAccelerator::Config::SpatialLayer& l,
    const ::media::VideoEncodeAccelerator::Config::SpatialLayer& r) {
  return (l.width == r.width && l.height == r.height &&
          l.bitrate_bps == r.bitrate_bps && l.framerate == r.framerate &&
          l.max_qp == r.max_qp &&
          l.num_of_temporal_layers == r.num_of_temporal_layers);
}

bool operator==(const ::media::VideoEncodeAccelerator::Config& l,
                const ::media::VideoEncodeAccelerator::Config& r) {
  return l.input_format == r.input_format &&
         l.input_visible_size == r.input_visible_size &&
         l.output_profile == r.output_profile &&
         l.initial_bitrate == r.initial_bitrate &&
         l.initial_framerate == r.initial_framerate &&
         l.gop_length == r.gop_length &&
         l.h264_output_level == r.h264_output_level &&
         l.storage_type == r.storage_type && l.content_type == r.content_type &&
         l.spatial_layers == r.spatial_layers;
}

TEST(VideoEncoderInfoStructTraitTest, RoundTrip) {
  ::media::VideoEncoderInfo input;
  input.implementation_name = "FakeVideoEncodeAccelerator";
  // Scaling settings.
  input.scaling_settings = ::media::ScalingSettings(12, 123);
  // FPS allocation.
  for (size_t i = 0; i < ::media::VideoEncoderInfo::kMaxSpatialLayers; ++i)
    input.fps_allocation[i] = {5, 5, 10};
  // Resolution bitrate limits.
  input.resolution_bitrate_limits.push_back(::media::ResolutionBitrateLimit(
      gfx::Size(123, 456), 123456, 123456, 789012));
  input.resolution_bitrate_limits.push_back(::media::ResolutionBitrateLimit(
      gfx::Size(789, 1234), 1234567, 1234567, 7890123));
  // Other bool values.
  input.supports_native_handle = true;
  input.has_trusted_rate_controller = true;
  input.is_hardware_accelerated = true;
  input.supports_simulcast = true;

  ::media::VideoEncoderInfo output = input;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::VideoEncoderInfo>(
      &input, &output));
  EXPECT_EQ(input, output);
}

TEST(SpatialLayerStructTraitTest, RoundTrip) {
  ::media::VideoEncodeAccelerator::Config::SpatialLayer input_spatial_layer;
  input_spatial_layer.width = 320;
  input_spatial_layer.width = 180;
  input_spatial_layer.bitrate_bps = 12345678u;
  input_spatial_layer.framerate = 24u;
  input_spatial_layer.max_qp = 30u;
  input_spatial_layer.num_of_temporal_layers = 3u;
  ::media::VideoEncodeAccelerator::Config::SpatialLayer output_spatial_layer;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<mojom::SpatialLayer>(
      &input_spatial_layer, &output_spatial_layer));
  EXPECT_EQ(input_spatial_layer, output_spatial_layer);
}

TEST(VideoEncodeAcceleratorConfigStructTraitTest, RoundTrip) {
  std::vector<::media::VideoEncodeAccelerator::Config::SpatialLayer>
      input_spatial_layers(3);
  gfx::Size kBaseSize(320, 180);
  uint32_t kBaseBitrateBps = 123456u;
  uint32_t kBaseFramerate = 24u;
  for (size_t i = 0; i < input_spatial_layers.size(); ++i) {
    input_spatial_layers[i].width =
        static_cast<int32_t>(kBaseSize.width() * (i + 1));
    input_spatial_layers[i].height =
        static_cast<int32_t>(kBaseSize.height() * (i + 1));
    input_spatial_layers[i].bitrate_bps = kBaseBitrateBps * (i + 1) / 2;
    input_spatial_layers[i].framerate = kBaseFramerate * 2 / (i + 1);
    input_spatial_layers[i].max_qp = 30 * (i + 1) / 2;
    input_spatial_layers[i].num_of_temporal_layers = 3 - i;
  }
  ::media::VideoEncodeAccelerator::Config input_config(
      ::media::PIXEL_FORMAT_NV12, kBaseSize, ::media::VP9PROFILE_PROFILE0,
      kBaseBitrateBps, kBaseFramerate, base::nullopt, base::nullopt,
      ::media::VideoEncodeAccelerator::Config::StorageType::kDmabuf,
      ::media::VideoEncodeAccelerator::Config::ContentType::kCamera,
      input_spatial_layers);
  DVLOG(4) << input_config.AsHumanReadableString();

  ::media::VideoEncodeAccelerator::Config output_config{};
  ASSERT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::VideoEncodeAcceleratorConfig>(
          &input_config, &output_config));
  DVLOG(4) << output_config.AsHumanReadableString();
  EXPECT_EQ(input_config, output_config);
}
}  // namespace media
