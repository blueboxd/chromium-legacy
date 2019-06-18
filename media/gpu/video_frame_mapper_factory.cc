// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/video_frame_mapper_factory.h"

#include "build/build_config.h"
#include "media/gpu/buildflags.h"

#if defined(OS_CHROMEOS)
#include "media/gpu/chromeos/generic_dmabuf_video_frame_mapper.h"
#endif  // defined(OS_CHROMEOS)

#if BUILDFLAG(USE_VAAPI) && defined(OS_LINUX)
#include "media/gpu/vaapi/vaapi_dmabuf_video_frame_mapper.h"
#endif  // BUILDFLAG(USE_VAAPI) && defined(OS_LINUX)

namespace media {

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper(
    VideoPixelFormat format) {
#if BUILDFLAG(USE_VAAPI) && defined(OS_LINUX)
  return CreateMapper(format, false);
#else
  return CreateMapper(format, true);
#endif  // BUILDFLAG(USE_VAAPI) && defined(OS_LINUX)
}

// static
std::unique_ptr<VideoFrameMapper> VideoFrameMapperFactory::CreateMapper(
    VideoPixelFormat format,
    bool linear_buffer_mapper) {
#if defined(OS_CHROMEOS)
  if (linear_buffer_mapper)
    return GenericDmaBufVideoFrameMapper::Create(format);
#endif  // defined(OS_LINUX)

#if BUILDFLAG(USE_VAAPI) && defined(OS_LINUX)
  return VaapiDmaBufVideoFrameMapper::Create(format);
#endif  // BUILDFLAG(USE_VAAPI) && defined(OS_LINUX)

  return nullptr;
}

}  // namespace media
