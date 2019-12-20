// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/mojom/video_encoder_info_mojom_traits.h"

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

TEST(VideoEncoderInfoStructTraitTest, RoundTrip) {
  ::media::VideoEncoderInfo input;
  input.implementation_name = "FakeVideoEncodeAccelerator";
  // Scaling settings.
  input.scaling_settings.min_qp = 12;
  input.scaling_settings.max_qp = 123;
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
}  // namespace media
