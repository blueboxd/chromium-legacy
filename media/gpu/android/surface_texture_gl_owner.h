// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_SURFACE_TEXTURE_GL_OWNER_H_
#define MEDIA_GPU_ANDROID_SURFACE_TEXTURE_GL_OWNER_H_

#include "media/gpu/android/texture_owner.h"

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gl/android/surface_texture.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace media {

// This class wraps the Surface Texture usage. It is used to create a surface
// texture attached to a new texture of the current platform GL context. The
// surface handle of the SurfaceTexture is attached to the decoded media
// frames. Media frames can update the attached surface handle with image data.
// This class helps to update the attached texture using that image data
// present in the surface.
class MEDIA_GPU_EXPORT SurfaceTextureGLOwner : public TextureOwner {
 public:
  gl::GLContext* GetContext() const override;
  gl::GLSurface* GetSurface() const override;
  void SetFrameAvailableCallback(
      const base::RepeatingClosure& frame_available_cb) override;
  gl::ScopedJavaSurface CreateJavaSurface() const override;
  void UpdateTexImage() override;
  void EnsureTexImageBound() override;
  void GetTransformMatrix(float mtx[16]) override;
  void ReleaseBackBuffers() override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;

 protected:
  void OnTextureDestroyed(gpu::gles2::AbstractTexture*) override;

 private:
  friend class TextureOwner;

  SurfaceTextureGLOwner(std::unique_ptr<gpu::gles2::AbstractTexture> texture);
  ~SurfaceTextureGLOwner() override;

  scoped_refptr<gl::SurfaceTexture> surface_texture_;
  // The context and surface that were used to create |surface_texture_|.
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(SurfaceTextureGLOwner);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_SURFACE_TEXTURE_GL_OWNER_H_
