// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/texture_ref.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "media/gpu/test/video_frame_helpers.h"

#if defined(OS_LINUX)
#include <libdrm/drm_fourcc.h>

#include "media/gpu/linux/platform_video_frame_utils.h"
#endif

namespace media {
namespace test {

TextureRef::TextureRef(uint32_t texture_id,
                       base::OnceClosure no_longer_needed_cb)
    : texture_id_(texture_id),
      no_longer_needed_cb_(std::move(no_longer_needed_cb)) {}

TextureRef::~TextureRef() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(no_longer_needed_cb_).Run();
}

// static
scoped_refptr<TextureRef> TextureRef::Create(
    uint32_t texture_id,
    base::OnceClosure no_longer_needed_cb) {
  return base::WrapRefCounted(
      new TextureRef(texture_id, std::move(no_longer_needed_cb)));
}

// static
scoped_refptr<TextureRef> TextureRef::CreatePreallocated(
    uint32_t texture_id,
    base::OnceClosure no_longer_needed_cb,
    VideoPixelFormat pixel_format,
    const gfx::Size& size,
    gfx::BufferUsage buffer_usage) {
  scoped_refptr<TextureRef> texture_ref;
#if defined(OS_CHROMEOS)
  texture_ref = TextureRef::Create(texture_id, std::move(no_longer_needed_cb));
  LOG_ASSERT(texture_ref);
  // We set visible size to coded_size. The correct visible rectangle is set
  // later in ExportVideoFrame().
  texture_ref->frame_ =
      CreatePlatformVideoFrame(pixel_format, size, gfx::Rect(size), size,
                               base::TimeDelta(), buffer_usage);
#endif
  return texture_ref;
}

gfx::GpuMemoryBufferHandle TextureRef::ExportGpuMemoryBufferHandle() const {
#if defined(OS_CHROMEOS)
  auto handle = CreateGpuMemoryBufferHandle(frame_.get());
  DCHECK(!handle.is_null());
  return handle;
#else
  return gfx::GpuMemoryBufferHandle();
#endif
}

scoped_refptr<VideoFrame> TextureRef::ExportVideoFrame(
    gfx::Rect visible_rect) const {
#if defined(OS_CHROMEOS)
  return VideoFrame::WrapVideoFrame(*frame_, frame_->format(), visible_rect,
                                    visible_rect.size());
#else
  return nullptr;
#endif
}

}  // namespace test
}  // namespace media
