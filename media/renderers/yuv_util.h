// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_YUV_UTIL_H_
#define MEDIA_RENDERERS_YUV_UTIL_H_

#include "gpu/GLES2/gl2extchromium.h"
#include "media/base/media_export.h"
#include "media/base/video_types.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/color_space.h"

namespace gpu {
struct MailboxHolder;
}

namespace viz {
class RasterContextProvider;
}  // namespace viz

namespace media {

class VideoFrame;

// Converts a YUV video frame to RGB format and stores the results in the
// provided mailbox. The caller of this function maintains ownership of the
// mailbox. Automatically handles upload of CPU memory backed VideoFrames in
// I420 format. VideoFrames that wrap external textures can be I420 or NV12
// format.
MEDIA_EXPORT void ConvertFromVideoFrameYUV(
    const VideoFrame* video_frame,
    viz::RasterContextProvider* raster_context_provider,
    const gpu::MailboxHolder& dest_mailbox_holder,
    unsigned int internal_format = GL_RGBA,
    unsigned int type = GL_UNSIGNED_BYTE,
    bool flip_y = false,
    bool use_visible_rect = false);

}  // namespace media

#endif  // MEDIA_RENDERERS_YUV_UTIL_H_