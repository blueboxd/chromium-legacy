// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_TEXTURE_REF_H_
#define MEDIA_GPU_TEST_TEXTURE_REF_H_

#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/buildflags.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {
class GpuMemoryBufferFactory;
}

namespace media {
namespace test {

// A helper class used to manage the lifetime of a Texture. Can be backed by
// either a buffer allocated by the VDA, or by a preallocated pixmap.
class TextureRef : public base::RefCounted<TextureRef> {
 public:
  static scoped_refptr<TextureRef> Create(
      uint32_t texture_id,
      base::OnceClosure no_longer_needed_cb);

  static scoped_refptr<TextureRef> CreatePreallocated(
      gpu::GpuMemoryBufferFactory* gpu_memory_buffer_factory,
      uint32_t texture_id,
      base::OnceClosure no_longer_needed_cb,
      VideoPixelFormat pixel_format,
      const gfx::Size& size,
      gfx::BufferUsage buffer_usage);

  gfx::GpuMemoryBufferHandle ExportGpuMemoryBufferHandle() const;
  scoped_refptr<VideoFrame> ExportVideoFrame(gfx::Rect visible_rect) const;

  int32_t texture_id() const { return texture_id_; }

 private:
  friend class base::RefCounted<TextureRef>;

  TextureRef(uint32_t texture_id, base::OnceClosure no_longer_needed_cb);
  ~TextureRef();

  uint32_t texture_id_;
  base::OnceClosure no_longer_needed_cb_;
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  scoped_refptr<VideoFrame> frame_;
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  THREAD_CHECKER(thread_checker_);
};

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_TEXTURE_REF_H_
