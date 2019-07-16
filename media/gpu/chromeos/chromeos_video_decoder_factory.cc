// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/chromeos_video_decoder_factory.h"

#include <utility>

#include "base/sequenced_task_runner.h"
#include "media/base/video_decoder.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/linux/mailbox_video_frame_converter.h"
#include "media/gpu/linux/platform_video_frame_pool.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_video_decoder.h"
#endif

#if BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/v4l2/v4l2_slice_video_decoder.h"
#endif

#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
#include "media/gpu/linux/video_decoder_pipeline.h"
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
  if (!client_task_runner || !frame_pool || !frame_converter)
    return nullptr;

  std::unique_ptr<VideoDecoder> decoder;

  // TODO(dstaessens@): We first try VAAPI as USE_V4L2_CODEC might also be
  // set, even though initialization of V4L2SliceVideoDecoder would fail. We
  // need to implement a better way to select the correct decoder.
#if BUILDFLAG(USE_VAAPI)
  decoder =
      VaapiVideoDecoder::Create(client_task_runner, std::move(frame_pool));
#elif BUILDFLAG(USE_V4L2_CODEC)
  decoder =
      V4L2SliceVideoDecoder::Create(client_task_runner, std::move(frame_pool));
#endif

#if BUILDFLAG(USE_VAAPI) || BUILDFLAG(USE_V4L2_CODEC)
  return std::make_unique<VideoDecoderPipeline>(std::move(client_task_runner),
                                                std::move(decoder),
                                                std::move(frame_converter));
#else
  return nullptr;
#endif
}

}  // namespace media
