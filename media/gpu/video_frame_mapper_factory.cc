// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/video_frame_mapper_factory.h"

#include "build/build_config.h"
#include "media/gpu/buildflags.h"

#if BUILDFLAG(USE_V4L2_CODEC) || BUILDFLAG(USE_VAAPI)
#include "media/gpu/linux/generic_dmabuf_video_frame_mapper.h"
#endif  // BUILDFLAG(USE_V4L2_CODEC) || BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_dmabuf_video_frame_mapper.h"
#endif  // BUILDFLAG(USE_VAAPI)

namespace media {

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper(
    VideoPixelFormat format) {
#if BUILDFLAG(USE_VAAPI)
  return CreateMapper(format, false);
#else
  return CreateMapper(format, true);
#endif  // BUILDFLAG(USE_VAAPI)
}

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper(
    VideoPixelFormat format,
    bool linear_buffer_mapper) {
#if BUILDFLAG(USE_V4L2_CODEC) || BUILDFLAG(USE_VAAPI)
  if (linear_buffer_mapper)
    return GenericDmaBufVideoFrameMapper::Create(format);
#endif  // BUILDFLAG(USE_V4L2_CODEC) || BUILDFLAG(USE_VAAPI)

#if BUILDFLAG(USE_VAAPI)
  return VaapiDmaBufVideoFrameMapper::Create(format);
#endif  // BUILDFLAG(USE_VAAPI)

  return nullptr;
}

}  // namespace media
