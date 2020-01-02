// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_

#include <dawn/dawn_proc_table.h>
#include <dawn/webgpu.h>

#include "base/callback_helpers.h"
#include "base/util/type_safety/pass_key.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"

typedef unsigned int GLenum;
class SkPromiseImageTexture;

namespace gl {
class GLImage;
}

namespace gpu {
namespace gles2 {
class Texture;
class TexturePassthrough;
}  // namespace gles2

enum class RepresentationAccessMode {
  kNone,
  kRead,
  kWrite,
};

// A representation of a SharedImageBacking for use with a specific use case /
// api.
class GPU_GLES2_EXPORT SharedImageRepresentation {
 public:
  SharedImageRepresentation(SharedImageManager* manager,
                            SharedImageBacking* backing,
                            MemoryTypeTracker* tracker);
  virtual ~SharedImageRepresentation();

  viz::ResourceFormat format() const { return backing_->format(); }
  const gfx::Size& size() const { return backing_->size(); }
  const gfx::ColorSpace& color_space() const { return backing_->color_space(); }
  uint32_t usage() const { return backing_->usage(); }
  MemoryTypeTracker* tracker() { return tracker_; }
  bool IsCleared() const { return backing_->IsCleared(); }
  void SetCleared() { backing_->SetCleared(); }
  gfx::Rect ClearedRect() const { return backing_->ClearedRect(); }
  void SetClearedRect(const gfx::Rect& cleared_rect) {
    backing_->SetClearedRect(cleared_rect);
  }

  // Indicates that the underlying graphics context has been lost, and the
  // backing should be treated as destroyed.
  void OnContextLost() {
    has_context_ = false;
    backing_->OnContextLost();
  }

 protected:
  SharedImageManager* manager() const { return manager_; }
  SharedImageBacking* backing() const { return backing_; }
  bool has_context() const { return has_context_; }

 private:
  SharedImageManager* const manager_;
  SharedImageBacking* const backing_;
  MemoryTypeTracker* const tracker_;
  bool has_context_ = true;
};

class SharedImageRepresentationFactoryRef : public SharedImageRepresentation {
 public:
  SharedImageRepresentationFactoryRef(SharedImageManager* manager,
                                      SharedImageBacking* backing,
                                      MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  const Mailbox& mailbox() const { return backing()->mailbox(); }
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) {
    backing()->Update(std::move(in_fence));
    backing()->OnWriteSucceeded();
  }
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) {
    return backing()->ProduceLegacyMailbox(mailbox_manager);
  }
  bool PresentSwapChain() { return backing()->PresentSwapChain(); }
};

class GPU_GLES2_EXPORT SharedImageRepresentationGLTextureBase
    : public SharedImageRepresentation {
 public:
  class ScopedAccess {
   public:
    ScopedAccess(util::PassKey<SharedImageRepresentationGLTextureBase> pass_key,
                 SharedImageRepresentationGLTextureBase* representation)
        : representation_(representation) {}
    ~ScopedAccess() { representation_->EndAccess(); }

   private:
    SharedImageRepresentationGLTextureBase* representation_ = nullptr;

    DISALLOW_COPY_AND_ASSIGN(ScopedAccess);
  };

  SharedImageRepresentationGLTextureBase(SharedImageManager* manager,
                                         SharedImageBacking* backing,
                                         MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  std::unique_ptr<ScopedAccess> BeginScopedAccess(GLenum mode);

 protected:
  friend class SharedImageRepresentationSkiaGL;

  // TODO(ericrk): Make these pure virtual and ensure real implementations
  // exist.
  virtual bool BeginAccess(GLenum mode);
  virtual void EndAccess() {}
};

class GPU_GLES2_EXPORT SharedImageRepresentationGLTexture
    : public SharedImageRepresentationGLTextureBase {
 public:
  SharedImageRepresentationGLTexture(SharedImageManager* manager,
                                     SharedImageBacking* backing,
                                     MemoryTypeTracker* tracker)
      : SharedImageRepresentationGLTextureBase(manager, backing, tracker) {}

  // TODO(ericrk): Move this to the ScopedAccess object. crbug.com/1003686
  virtual gles2::Texture* GetTexture() = 0;
};

class GPU_GLES2_EXPORT SharedImageRepresentationGLTexturePassthrough
    : public SharedImageRepresentationGLTextureBase {
 public:
  SharedImageRepresentationGLTexturePassthrough(SharedImageManager* manager,
                                                SharedImageBacking* backing,
                                                MemoryTypeTracker* tracker)
      : SharedImageRepresentationGLTextureBase(manager, backing, tracker) {}

  // TODO(ericrk): Move this to the ScopedAccess object. crbug.com/1003686
  virtual const scoped_refptr<gles2::TexturePassthrough>&
  GetTexturePassthrough() = 0;
};

class GPU_GLES2_EXPORT SharedImageRepresentationSkia
    : public SharedImageRepresentation {
 public:
  class GPU_GLES2_EXPORT ScopedWriteAccess {
   public:
    ScopedWriteAccess(util::PassKey<SharedImageRepresentationSkia> pass_key,
                      SharedImageRepresentationSkia* representation,
                      sk_sp<SkSurface> surface);
    ~ScopedWriteAccess();

    SkSurface* surface() const { return surface_.get(); }

   private:
    SharedImageRepresentationSkia* const representation_;
    sk_sp<SkSurface> surface_;

    DISALLOW_COPY_AND_ASSIGN(ScopedWriteAccess);
  };

  class GPU_GLES2_EXPORT ScopedReadAccess {
   public:
    ScopedReadAccess(util::PassKey<SharedImageRepresentationSkia> pass_key,
                     SharedImageRepresentationSkia* representation,
                     sk_sp<SkPromiseImageTexture> promise_image_texture);
    ~ScopedReadAccess();

    SkPromiseImageTexture* promise_image_texture() const {
      return promise_image_texture_.get();
    }

   private:
    SharedImageRepresentationSkia* const representation_;
    sk_sp<SkPromiseImageTexture> promise_image_texture_;

    DISALLOW_COPY_AND_ASSIGN(ScopedReadAccess);
  };

  SharedImageRepresentationSkia(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  // Note: See BeginWriteAccess below for a description of the semaphore
  // parameters.
  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);
  std::unique_ptr<ScopedWriteAccess> BeginScopedWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);

  // Note: See BeginReadAccess below for a description of the semaphore
  // parameters.
  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores);

  virtual bool SupportsMultipleConcurrentReadAccess();

 protected:
  // Begin the write access. The implementations should insert semaphores into
  // begin_semaphores vector which client will wait on before writing the
  // backing. The ownership of begin_semaphores will be passed to client.
  // The implementations should also insert semaphores into end_semaphores,
  // client must submit them with drawing operations which use the backing.
  // The ownership of end_semaphores are not passed to client. And client must
  // submit the end_semaphores before calling EndWriteAccess().
  virtual sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) = 0;
  virtual void EndWriteAccess(sk_sp<SkSurface> surface) = 0;

  // Begin the read access. The implementations should insert semaphores into
  // begin_semaphores vector which client will wait on before reading the
  // backing. The ownership of begin_semaphores will be passed to client.
  // The implementations should also insert semaphores into end_semaphores,
  // client must submit them with drawing operations which use the backing.
  // The ownership of end_semaphores are not passed to client. And client must
  // submit the end_semaphores before calling EndReadAccess().
  virtual sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) = 0;
  virtual void EndReadAccess() = 0;
};

class GPU_GLES2_EXPORT SharedImageRepresentationDawn
    : public SharedImageRepresentation {
 public:
  SharedImageRepresentationDawn(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  class GPU_GLES2_EXPORT ScopedAccess {
   public:
    ScopedAccess(util::PassKey<SharedImageRepresentationDawn> pass_key,
                 SharedImageRepresentationDawn* representation,
                 WGPUTexture texture);
    ~ScopedAccess();

    WGPUTexture texture() const { return texture_; }

   private:
    SharedImageRepresentationDawn* representation_ = nullptr;
    WGPUTexture texture_ = 0;

    DISALLOW_COPY_AND_ASSIGN(ScopedAccess);
  };

  // Calls BeginAccess and returns a ScopedAccess object which will EndAccess
  // when it goes out of scope. The Representation must outlive the returned
  // ScopedAccess.
  std::unique_ptr<ScopedAccess> BeginScopedAccess(WGPUTextureUsage usage);

 private:
  // This can return null in case of a Dawn validation error, for example if
  // usage is invalid.
  virtual WGPUTexture BeginAccess(WGPUTextureUsage usage) = 0;
  virtual void EndAccess() = 0;
};

class GPU_GLES2_EXPORT SharedImageRepresentationOverlay
    : public SharedImageRepresentation {
 public:
  SharedImageRepresentationOverlay(SharedImageManager* manager,
                                   SharedImageBacking* backing,
                                   MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  class ScopedReadAccess {
   public:
    ScopedReadAccess(util::PassKey<SharedImageRepresentationOverlay> pass_key,
                     SharedImageRepresentationOverlay* representation,
                     gl::GLImage* gl_image)
        : representation_(representation), gl_image_(gl_image) {}
    ~ScopedReadAccess() {
      if (representation_)
        representation_->EndReadAccess();
    }

    gl::GLImage* gl_image() const {
      DCHECK(representation_);
      return gl_image_;
    }

   private:
    SharedImageRepresentationOverlay* representation_;
    gl::GLImage* gl_image_;
  };

#if defined(OS_ANDROID)
  virtual void NotifyOverlayPromotion(bool promotion,
                                      const gfx::Rect& bounds) = 0;
#endif

  std::unique_ptr<ScopedReadAccess> BeginScopedReadAccess(bool needs_gl_image);

 protected:
  // TODO(weiliangc): Currently this only handles Android pre-SurfaceControl
  // case. Add appropriate fence later.
  virtual void BeginReadAccess() = 0;
  virtual void EndReadAccess() = 0;

  // TODO(weiliangc): Add API to backing AHardwareBuffer.

  // TODO(penghuang): Refactor it to not depend on GL.
  // Get the backing as GLImage for GLSurface::ScheduleOverlayPlane.
  virtual gl::GLImage* GetGLImage() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_
