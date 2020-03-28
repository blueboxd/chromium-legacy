// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_YUV_UTIL_H_
#define MEDIA_RENDERERS_YUV_UTIL_H_

#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"

// Skia forward declarations
class GrBackendTexture;
class GrContext;
class SkImage;

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class VideoFrame;

// Converts a YUV video frame to RGB format and stored the results in the GL
// texture with ID |texture_out_id|. The caller of this function maintains
// ownership of the out texture.
MEDIA_EXPORT void ConvertFromVideoFrameYUVTextures(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    unsigned int texture_out_target,
    unsigned int texture_out_id);

MEDIA_EXPORT sk_sp<SkImage>
NewSkImageFromVideoFrameYUVTexturesWithExternalBackend(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    unsigned int texture_target,
    unsigned int texture_id);

MEDIA_EXPORT sk_sp<SkImage> YUVGrBackendTexturesToSkImage(
    GrContext* gr_context,
    gfx::ColorSpace video_color_space,
    VideoPixelFormat video_format,
    GrBackendTexture* yuv_textures,
    const GrBackendTexture& result_texture);

}  // namespace media

#endif  // MEDIA_RENDERERS_YUV_UTIL_H_