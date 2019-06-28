// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_IMAGE_READER_GL_OWNER_H_
#define MEDIA_GPU_ANDROID_IMAGE_READER_GL_OWNER_H_

#include <memory>

#include "base/android/android_image_reader_compat.h"
#include "base/containers/flat_map.h"
#include "media/gpu/android/texture_owner.h"
#include "ui/gl/gl_fence_egl.h"
#include "ui/gl/gl_image_ahardwarebuffer.h"

namespace base {
namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace media {

struct FrameAvailableEvent_ImageReader;

// This class wraps the AImageReader usage and is used to create a GL texture
// using the current platform GL context and returns a new ImageReaderGLOwner
// attached to it. The surface handle of the AImageReader is attached to
// decoded media frames. Media frames can update the attached surface handle
// with image data and this class helps to create an eglImage using that image
// data present in the surface.
class MEDIA_GPU_EXPORT ImageReaderGLOwner : public TextureOwner {
 public:
  gl::GLContext* GetContext() const override;
  gl::GLSurface* GetSurface() const override;
  gl::ScopedJavaSurface CreateJavaSurface() const override;
  void UpdateTexImage() override;
  void EnsureTexImageBound() override;
  void GetTransformMatrix(float mtx[16]) override;
  void ReleaseBackBuffers() override;
  void SetReleaseTimeToNow() override;
  void IgnorePendingRelease() override;
  bool IsExpectingFrameAvailable() override;
  void WaitForFrameAvailable() override;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
  GetAHardwareBuffer() override;

  const AImageReader* image_reader_for_testing() const { return image_reader_; }
  int32_t max_images_for_testing() const { return max_images_; }

 protected:
  void OnTextureDestroyed(gpu::gles2::AbstractTexture*) override;

 private:
  friend class TextureOwner;
  class ScopedHardwareBufferImpl;

  // Manages ownership of the latest image retrieved from AImageReader and
  // ensuring synchronization of its use in GL using fences.
  class ScopedCurrentImageRef {
   public:
    ScopedCurrentImageRef(ImageReaderGLOwner* texture_owner,
                          AImage* image,
                          base::ScopedFD ready_fence);
    ~ScopedCurrentImageRef();
    AImage* image() const { return image_; }
    base::ScopedFD GetReadyFence() const;
    void EnsureBound();

   private:
    ImageReaderGLOwner* texture_owner_;
    AImage* image_;
    base::ScopedFD ready_fence_;

    // Set to true if the current image is bound to |texture_id_|.
    bool image_bound_ = false;

    DISALLOW_COPY_AND_ASSIGN(ScopedCurrentImageRef);
  };

  ImageReaderGLOwner(std::unique_ptr<gpu::gles2::AbstractTexture> texture,
                     Mode secure_mode);
  ~ImageReaderGLOwner() override;

  // Registers and releases a ref on the image. Once the ref-count for an image
  // goes to 0, it is released back to the AImageReader with an optional release
  // fence if needed.
  void RegisterRefOnImage(AImage* image);
  void ReleaseRefOnImage(AImage* image, base::ScopedFD fence_fd);

  // AImageReader instance
  AImageReader* image_reader_;

  // Most recently acquired image using image reader. This works like a cached
  // image until next new image is acquired which overwrites this.
  base::Optional<ScopedCurrentImageRef> current_image_ref_;
  std::unique_ptr<AImageReader_ImageListener> listener_;

  // A map consisting of pending refs on an AImage. If an image has any refs, it
  // is automatically released once the ref-count is 0.
  struct ImageRef {
    ImageRef();
    ~ImageRef();

    ImageRef(ImageRef&& other);
    ImageRef& operator=(ImageRef&& other);

    size_t count = 0u;
    base::ScopedFD release_fence_fd;

    DISALLOW_COPY_AND_ASSIGN(ImageRef);
  };
  using AImageRefMap = base::flat_map<AImage*, ImageRef>;
  AImageRefMap image_refs_;

  // reference to the class instance which is used to dynamically
  // load the functions in android libraries at runtime.
  base::android::AndroidImageReader& loader_;

  // The context and surface that were used to create |texture_id_|.
  scoped_refptr<gl::GLContext> context_;
  scoped_refptr<gl::GLSurface> surface_;

  // When SetReleaseTimeToNow() was last called. i.e., when the last
  // codec buffer was released to this surface. Or null if
  // IgnorePendingRelease() or WaitForFrameAvailable() have been called since.
  base::TimeTicks release_time_;
  scoped_refptr<FrameAvailableEvent_ImageReader> frame_available_event_;
  int32_t max_images_ = 0;

  THREAD_CHECKER(thread_checker_);
  DISALLOW_COPY_AND_ASSIGN(ImageReaderGLOwner);
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_IMAGE_READER_GL_OWNER_H_
