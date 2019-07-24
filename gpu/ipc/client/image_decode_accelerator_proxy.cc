// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/client/image_decode_accelerator_proxy.h"

#include <string.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/config/gpu_info.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "gpu/ipc/common/command_buffer_id.h"
#include "gpu/ipc/common/gpu_messages.h"
#include "media/parsers/jpeg_parser.h"
#include "media/parsers/vp8_parser.h"
#include "media/parsers/webp_parser.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

namespace {

bool IsJpegImage(base::span<const uint8_t> encoded_data) {
  if (encoded_data.size() < 3u)
    return false;
  return memcmp("\xFF\xD8\xFF", encoded_data.data(), 3u) == 0;
}

ImageDecodeAcceleratorType GetImageType(
    base::span<const uint8_t> encoded_data) {
  static_assert(static_cast<int>(ImageDecodeAcceleratorType::kMaxValue) == 2,
                "GetImageType() must be adapted to support all image types in "
                "ImageDecodeAcceleratorType");

  if (IsJpegImage(encoded_data))
    return ImageDecodeAcceleratorType::kJpeg;

  if (media::IsLossyWebPImage(encoded_data))
    return ImageDecodeAcceleratorType::kWebP;

  return ImageDecodeAcceleratorType::kUnknown;
}

bool GetJpegSubsampling(const media::JpegParseResult& parse_result,
                        ImageDecodeAcceleratorSubsampling* subsampling) {
  static_assert(
      static_cast<int>(ImageDecodeAcceleratorSubsampling::kMaxValue) == 2,
      "GetJpegSubsampling() must be adapted to support all "
      "subsampling factors in ImageDecodeAcceleratorSubsampling");

  // Currently, only 3 components are supported (this excludes, for example,
  // grayscale and CMYK JPEGs).
  if (parse_result.frame_header.num_components != 3u)
    return false;

  const uint8_t comp0_h =
      parse_result.frame_header.components[0].horizontal_sampling_factor;
  const uint8_t comp0_v =
      parse_result.frame_header.components[0].vertical_sampling_factor;
  const uint8_t comp1_h =
      parse_result.frame_header.components[1].horizontal_sampling_factor;
  const uint8_t comp1_v =
      parse_result.frame_header.components[1].vertical_sampling_factor;
  const uint8_t comp2_h =
      parse_result.frame_header.components[2].horizontal_sampling_factor;
  const uint8_t comp2_v =
      parse_result.frame_header.components[2].vertical_sampling_factor;

  if (comp1_h != 1u || comp1_v != 1u || comp2_h != 1u || comp2_v != 1u)
    return false;

  if (comp0_h == 2u && comp0_v == 2u) {
    *subsampling = ImageDecodeAcceleratorSubsampling::k420;
    return true;
  } else if (comp0_h == 2u && comp0_v == 1u) {
    *subsampling = ImageDecodeAcceleratorSubsampling::k422;
    return true;
  } else if (comp0_h == 1u && comp0_v == 1u) {
    *subsampling = ImageDecodeAcceleratorSubsampling::k444;
    return true;
  }
  return false;
}

bool IsSupportedJpegImage(
    base::span<const uint8_t> encoded_data,
    const ImageDecodeAcceleratorSupportedProfile& supported_profile) {
  DCHECK(IsJpegImage(encoded_data));
  DCHECK_EQ(ImageDecodeAcceleratorType::kJpeg, supported_profile.image_type);

  // First, parse the JPEG file. This fails for progressive JPEGs (which we
  // don't support anyway).
  media::JpegParseResult parse_result;
  if (!media::ParseJpegPicture(encoded_data.data(), encoded_data.size(),
                               &parse_result)) {
    return false;
  }

  // Now, check the chroma subsampling format.
  ImageDecodeAcceleratorSubsampling subsampling;
  if (!GetJpegSubsampling(parse_result, &subsampling))
    return false;
  if (std::find(supported_profile.subsamplings.cbegin(),
                supported_profile.subsamplings.cend(),
                subsampling) == supported_profile.subsamplings.cend()) {
    return false;
  }

  // Now, check the dimensions.
  const int encoded_width =
      base::strict_cast<int>(parse_result.frame_header.coded_width);
  const int encoded_height =
      base::strict_cast<int>(parse_result.frame_header.coded_height);
  if (encoded_width < supported_profile.min_encoded_dimensions.width() ||
      encoded_height < supported_profile.min_encoded_dimensions.height() ||
      encoded_width > supported_profile.max_encoded_dimensions.width() ||
      encoded_height > supported_profile.max_encoded_dimensions.height()) {
    return false;
  }

  return true;
}

bool IsSupportedWebPImage(
    base::span<const uint8_t> encoded_data,
    const ImageDecodeAcceleratorSupportedProfile& supported_profile) {
  DCHECK(media::IsLossyWebPImage(encoded_data));
  DCHECK_EQ(ImageDecodeAcceleratorType::kWebP, supported_profile.image_type);

  const std::unique_ptr<media::Vp8FrameHeader> parse_result =
      media::ParseWebPImage(encoded_data);
  if (!parse_result)
    return false;

  // TODO(crbug.com/984971): we may need to compute the coded size instead and
  // check that against the supported dimensions.
  const int width = base::strict_cast<int>(parse_result->width);
  const int height = base::strict_cast<int>(parse_result->height);
  return width >= supported_profile.min_encoded_dimensions.width() &&
         height >= supported_profile.min_encoded_dimensions.height() &&
         width <= supported_profile.max_encoded_dimensions.width() &&
         height <= supported_profile.max_encoded_dimensions.height();
}

}  // namespace

ImageDecodeAcceleratorProxy::ImageDecodeAcceleratorProxy(GpuChannelHost* host,
                                                         int32_t route_id)
    : host_(host), route_id_(route_id) {}

ImageDecodeAcceleratorProxy::~ImageDecodeAcceleratorProxy() {}

bool ImageDecodeAcceleratorProxy::IsImageSupported(
    base::span<const uint8_t> encoded_data) const {
  DCHECK(host_);

  const ImageDecodeAcceleratorType image_type = GetImageType(encoded_data);
  if (image_type == ImageDecodeAcceleratorType::kUnknown)
    return false;

  // Find the image decode accelerator supported profile according to the type
  // of the image.
  const std::vector<ImageDecodeAcceleratorSupportedProfile>& profiles =
      host_->gpu_info().image_decode_accelerator_supported_profiles;
  auto profile_it = std::find_if(
      profiles.cbegin(), profiles.cend(),
      [image_type](const ImageDecodeAcceleratorSupportedProfile& profile) {
        return profile.image_type == image_type;
      });
  if (profile_it == profiles.cend())
    return false;

  // Validate the image according to that profile.
  switch (image_type) {
    case ImageDecodeAcceleratorType::kJpeg:
      return IsSupportedJpegImage(encoded_data, *profile_it);
    case ImageDecodeAcceleratorType::kWebP:
      return IsSupportedWebPImage(encoded_data, *profile_it);
    case ImageDecodeAcceleratorType::kUnknown:
      // No Op. Should not reach due to a check above.
      break;
  }
  return false;
}

SyncToken ImageDecodeAcceleratorProxy::ScheduleImageDecode(
    base::span<const uint8_t> encoded_data,
    const gfx::Size& output_size,
    CommandBufferId raster_decoder_command_buffer_id,
    uint32_t transfer_cache_entry_id,
    int32_t discardable_handle_shm_id,
    uint32_t discardable_handle_shm_offset,
    uint64_t discardable_handle_release_count,
    const gfx::ColorSpace& target_color_space,
    bool needs_mips) {
  DCHECK(host_);
  DCHECK_EQ(host_->channel_id(),
            ChannelIdFromCommandBufferId(raster_decoder_command_buffer_id));
  DCHECK(IsImageSupported(encoded_data));

  GpuChannelMsg_ScheduleImageDecode_Params params;
  params.encoded_data =
      std::vector<uint8_t>(encoded_data.cbegin(), encoded_data.cend());
  params.output_size = output_size;
  params.raster_decoder_route_id =
      RouteIdFromCommandBufferId(raster_decoder_command_buffer_id);
  params.transfer_cache_entry_id = transfer_cache_entry_id;
  params.discardable_handle_shm_id = discardable_handle_shm_id;
  params.discardable_handle_shm_offset = discardable_handle_shm_offset;
  params.discardable_handle_release_count = discardable_handle_release_count;
  params.target_color_space = target_color_space;
  params.needs_mips = needs_mips;

  base::AutoLock lock(lock_);
  const uint64_t release_count = ++next_release_count_;
  // Note: we send the message under the lock to guarantee monotonicity of the
  // release counts as seen by the service.
  // The EnsureFlush() call makes sure that the sync token corresponding to
  // |discardable_handle_release_count| is visible to the service before
  // processing the image decode request.
  host_->EnsureFlush(UINT32_MAX);
  host_->Send(new GpuChannelMsg_ScheduleImageDecode(
      route_id_, std::move(params), release_count));
  return SyncToken(
      CommandBufferNamespace::GPU_IO,
      CommandBufferIdFromChannelAndRoute(host_->channel_id(), route_id_),
      release_count);
}

}  // namespace gpu
