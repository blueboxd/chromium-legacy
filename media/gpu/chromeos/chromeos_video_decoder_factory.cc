// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/chromeos_video_decoder_factory.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "media/base/video_decoder.h"
#include "media/gpu/buildflags.h"

#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
#include "media/gpu/linux/mailbox_video_frame_converter.h"
#include "media/gpu/linux/platform_video_frame_pool.h"
#include "media/gpu/linux/video_decoder_pipeline.h"
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_video_decoder.h"
#endif

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_slice_video_decoder.h"
#endif

namespace media {

// static
SupportedVideoDecoderConfigs
ChromeosVideoDecoderFactory::GetSupportedConfigs() {
  SupportedVideoDecoderConfigs supported_configs;
  SupportedVideoDecoderConfigs configs;

#if BUILDFLAG(USE_VAAPI)
  configs = VaapiVideoDecoder::GetSupportedConfigs();
  supported_configs.insert(supported_configs.end(), configs.begin(),
                           configs.end());
#endif  // BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(USE_V4L2_CODEC)
  configs = V4L2SliceVideoDecoder::GetSupportedConfigs();
  supported_configs.insert(supported_configs.end(), configs.begin(),
                           configs.end());
#endif  // BUILDFLAG(USE_V4L2_CODEC)

  return supported_configs;
}

// static
std::unique_ptr<VideoDecoder> ChromeosVideoDecoderFactory::Create(
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    std::unique_ptr<DmabufVideoFramePool> frame_pool,
    std::unique_ptr<VideoFrameConverter> frame_converter) {
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  // TODO(akahuang): Remove ChromeosVideoDecoderFactory.
  return VideoDecoderPipeline::Create(std::move(client_task_runner),
                                      std::move(frame_pool),
                                      std::move(frame_converter));
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

  return nullptr;
}

}  // namespace media
