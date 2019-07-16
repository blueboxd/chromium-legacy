// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_

#include <dawn/dawn.h>

#include "base/callback_helpers.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"

typedef unsigned int GLenum;
class SkPromiseImageTexture;

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
  }
#if defined(OS_WIN)
  void PresentSwapChain() { backing()->PresentSwapChain(); }
#endif  // OS_WIN
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) {
    return backing()->ProduceLegacyMailbox(mailbox_manager);
  }
};

class GPU_GLES2_EXPORT SharedImageRepresentationGLTexture
    : public SharedImageRepresentation {
 public:
  class ScopedAccess {
   public:
    ScopedAccess(SharedImageRepresentationGLTexture* representation,
                 GLenum mode)
        : representation_(representation),
          success_(representation_->BeginAccess(mode)) {}
    ~ScopedAccess() {
      if (success_)
        representation_->EndAccess();
    }

    bool success() const { return success_; }

   private:
    SharedImageRepresentationGLTexture* representation_;
    bool success_;
  };

  SharedImageRepresentationGLTexture(SharedImageManager* manager,
                                     SharedImageBacking* backing,
                                     MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  virtual gles2::Texture* GetTexture() = 0;

  // TODO(ericrk): Make these pure virtual and ensure real implementations
  // exist.
  virtual bool BeginAccess(GLenum mode);
  virtual void EndAccess() {}
};

class GPU_GLES2_EXPORT SharedImageRepresentationGLTexturePassthrough
    : public SharedImageRepresentation {
 public:
  class ScopedAccess {
   public:
    ScopedAccess(SharedImageRepresentationGLTexturePassthrough* representation,
                 GLenum mode)
        : representation_(representation),
          success_(representation_->BeginAccess(mode)) {}
    ~ScopedAccess() {
      if (success_)
        representation_->EndAccess();
    }

    bool success() const { return success_; }

   private:
    SharedImageRepresentationGLTexturePassthrough* representation_;
    bool success_;
  };

  SharedImageRepresentationGLTexturePassthrough(SharedImageManager* manager,
                                                SharedImageBacking* backing,
                                                MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  virtual const scoped_refptr<gles2::TexturePassthrough>&
  GetTexturePassthrough() = 0;

  // TODO(ericrk): Make these pure virtual and ensure real implementations
  // exist.
  virtual bool BeginAccess(GLenum mode);
  virtual void EndAccess() {}
};

class SharedImageRepresentationSkia : public SharedImageRepresentation {
 public:
  class ScopedWriteAccess {
   public:
    ScopedWriteAccess(SharedImageRepresentationSkia* representation,
                      int final_msaa_count,
                      const SkSurfaceProps& surface_props,
                      std::vector<GrBackendSemaphore>* begin_semaphores,
                      std::vector<GrBackendSemaphore>* end_semaphores);
    ScopedWriteAccess(SharedImageRepresentationSkia* representation,
                      std::vector<GrBackendSemaphore>* begin_semaphores,
                      std::vector<GrBackendSemaphore>* end_semaphores);
    ~ScopedWriteAccess();

    bool success() const { return !!surface_; }
    SkSurface* surface() const { return surface_.get(); }

   private:
    SharedImageRepresentationSkia* const representation_;
    sk_sp<SkSurface> surface_;
  };

  class ScopedReadAccess {
   public:
    ScopedReadAccess(SharedImageRepresentationSkia* representation,
                     std::vector<GrBackendSemaphore>* begin_semaphores,
                     std::vector<GrBackendSemaphore>* end_semaphores);
    ~ScopedReadAccess();

    bool success() const { return !!promise_image_texture_; }
    SkPromiseImageTexture* promise_image_texture() const {
      return promise_image_texture_.get();
    }

   private:
    SharedImageRepresentationSkia* const representation_;
    sk_sp<SkPromiseImageTexture> promise_image_texture_;
  };

  SharedImageRepresentationSkia(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

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

class SharedImageRepresentationDawn : public SharedImageRepresentation {
 public:
  SharedImageRepresentationDawn(SharedImageManager* manager,
                                SharedImageBacking* backing,
                                MemoryTypeTracker* tracker)
      : SharedImageRepresentation(manager, backing, tracker) {}

  // This can return null in case of a Dawn validation error, for example if
  // usage is invalid.
  virtual DawnTexture BeginAccess(DawnTextureUsageBit usage) = 0;
  virtual void EndAccess() = 0;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_REPRESENTATION_H_
