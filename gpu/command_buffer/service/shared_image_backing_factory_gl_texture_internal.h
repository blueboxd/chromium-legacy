// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_INTERNAL_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_INTERNAL_H_

#include "gpu/command_buffer/service/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image_backing_factory_gl_texture.h"
#include "gpu/command_buffer/service/shared_image_representation.h"

namespace gpu {

// Representation of a SharedImageBackingGLTexture or SharedImageBackingGLImage
// as a GL Texture.
class SharedImageRepresentationGLTextureImpl
    : public SharedImageRepresentationGLTexture {
 public:
  class Client {
   public:
    virtual bool OnGLTextureBeginAccess(GLenum mode) = 0;
  };
  SharedImageRepresentationGLTextureImpl(SharedImageManager* manager,
                                         SharedImageBacking* backing,
                                         Client* client,
                                         MemoryTypeTracker* tracker,
                                         gles2::Texture* texture);

 private:
  // SharedImageRepresentationGLTexture:
  gles2::Texture* GetTexture() override;
  bool BeginAccess(GLenum mode) override;

  Client* const client_ = nullptr;
  gles2::Texture* texture_;
};

// Representation of a SharedImageBackingGLTexture or
// SharedImageBackingGLTexturePassthrough as a GL TexturePassthrough.
class SharedImageRepresentationGLTexturePassthroughImpl
    : public SharedImageRepresentationGLTexturePassthrough {
 public:
  class Client {
   public:
    virtual bool OnGLTexturePassthroughBeginAccess(GLenum mode) = 0;
  };
  SharedImageRepresentationGLTexturePassthroughImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      Client* client,
      MemoryTypeTracker* tracker,
      scoped_refptr<gles2::TexturePassthrough> texture_passthrough);
  ~SharedImageRepresentationGLTexturePassthroughImpl() override;

 private:
  // SharedImageRepresentationGLTexturePassthrough:
  const scoped_refptr<gles2::TexturePassthrough>& GetTexturePassthrough()
      override;
  bool BeginAccess(GLenum mode) override;

  Client* const client_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> texture_passthrough_;
};

// Common helper functions for SharedImageBackingGLTexture and
// SharedImageBackingPassthroughGLImage.
class SharedImageBackingGLCommon : public SharedImageBacking {
 public:
  // These parameters are used to explicitly initialize a GL texture.
  // TODO(https://crbug.com/1092155): The goal here is to cache these parameters
  // (which are specified at initialization), so that the GL texture can be
  // allocated and bound lazily. In that world, |service_id| will not be a
  // parameter, but will be allocated lazily, and |image| will be handled by the
  // relevant sub-class.
  struct InitializeGLTextureParams {
    GLenum target = 0;
    GLenum internal_format = 0;
    GLenum format = 0;
    GLenum type = 0;
    bool is_cleared = false;
    bool is_rgb_emulation = false;
    bool framebuffer_attachment_angle = false;
    bool has_immutable_storage = false;
  };

  // Helper function to create a GL texture.
  static void MakeTextureAndSetParameters(
      GLenum target,
      GLuint service_id,
      bool framebuffer_attachment_angle,
      scoped_refptr<gles2::TexturePassthrough>* passthrough_texture,
      gles2::Texture** texture);
};

// Skia representation for both SharedImageBackingGLCommon.
class SharedImageRepresentationSkiaImpl : public SharedImageRepresentationSkia {
 public:
  class Client {
   public:
    virtual bool OnSkiaBeginReadAccess() = 0;
    virtual bool OnSkiaBeginWriteAccess() = 0;
  };
  SharedImageRepresentationSkiaImpl(
      SharedImageManager* manager,
      SharedImageBacking* backing,
      Client* client,
      scoped_refptr<SharedContextState> context_state,
      sk_sp<SkPromiseImageTexture> promise_texture,
      MemoryTypeTracker* tracker);
  ~SharedImageRepresentationSkiaImpl() override;

  void SetBeginReadAccessCallback(
      base::RepeatingClosure begin_read_access_callback);

 private:
  // SharedImageRepresentationSkia:
  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndWriteAccess(sk_sp<SkSurface> surface) override;
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndReadAccess() override;
  bool SupportsMultipleConcurrentReadAccess() override;

  void CheckContext();

  Client* const client_ = nullptr;
  scoped_refptr<SharedContextState> context_state_;
  sk_sp<SkPromiseImageTexture> promise_texture_;

  SkSurface* write_surface_ = nullptr;
#if DCHECK_IS_ON()
  gl::GLContext* context_ = nullptr;
#endif
};

// Implementation of SharedImageBacking that creates a GL Texture that is not
// backed by a GLImage.
class SharedImageBackingGLTexture : public SharedImageBacking {
 public:
  SharedImageBackingGLTexture(const Mailbox& mailbox,
                              viz::ResourceFormat format,
                              const gfx::Size& size,
                              const gfx::ColorSpace& color_space,
                              uint32_t usage,
                              bool is_passthrough);
  SharedImageBackingGLTexture(const SharedImageBackingGLTexture&) = delete;
  SharedImageBackingGLTexture& operator=(const SharedImageBackingGLTexture&) =
      delete;
  ~SharedImageBackingGLTexture() override;

  void InitializeGLTexture(
      GLuint service_id,
      const SharedImageBackingGLCommon::InitializeGLTextureParams& params);
  void SetCompatibilitySwizzle(
      const gles2::Texture::CompatibilitySwizzle* swizzle);

  GLenum GetGLTarget() const;
  GLuint GetGLServiceId() const;

 private:
  // SharedImageBacking:
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;
  gfx::Rect ClearedRect() const final;
  void SetClearedRect(const gfx::Rect& cleared_rect) final;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) final;
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) final;
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device) final;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

  bool IsPassthrough() const { return is_passthrough_; }

  const bool is_passthrough_;
  gles2::Texture* texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;

  sk_sp<SkPromiseImageTexture> cached_promise_texture_;
};

// Implementation of SharedImageBacking that creates a GL Texture that is backed
// by a GLImage and stores it as a gles2::Texture. Can be used with the legacy
// mailbox implementation.
class SharedImageBackingGLImage
    : public SharedImageBacking,
      public SharedImageRepresentationGLTextureImpl::Client,
      public SharedImageRepresentationGLTexturePassthroughImpl::Client,
      public SharedImageRepresentationSkiaImpl::Client {
 public:
  SharedImageBackingGLImage(
      scoped_refptr<gl::GLImage> image,
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage,
      const SharedImageBackingGLCommon::InitializeGLTextureParams& params,
      const SharedImageBackingFactoryGLTexture::UnpackStateAttribs& attribs,
      bool is_passthrough);
  SharedImageBackingGLImage(const SharedImageBackingGLImage& other) = delete;
  SharedImageBackingGLImage& operator=(const SharedImageBackingGLImage& other) =
      delete;
  ~SharedImageBackingGLImage() override;

  bool InitializeGLTexture();
  void InitializePixels(GLenum format, GLenum type, const uint8_t* data);

  GLenum GetGLTarget() const;
  GLuint GetGLServiceId() const;

 private:
  // SharedImageBacking:
  scoped_refptr<gfx::NativePixmap> GetNativePixmap() override;
  void OnMemoryDump(const std::string& dump_name,
                    base::trace_event::MemoryAllocatorDump* dump,
                    base::trace_event::ProcessMemoryDump* pmd,
                    uint64_t client_tracing_id) override;
  gfx::Rect ClearedRect() const final;
  void SetClearedRect(const gfx::Rect& cleared_rect) final;
  bool ProduceLegacyMailbox(MailboxManager* mailbox_manager) final;
  std::unique_ptr<SharedImageRepresentationGLTexture> ProduceGLTexture(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
  ProduceGLTexturePassthrough(SharedImageManager* manager,
                              MemoryTypeTracker* tracker) final;
  std::unique_ptr<SharedImageRepresentationOverlay> ProduceOverlay(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker) final;
  std::unique_ptr<SharedImageRepresentationDawn> ProduceDawn(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      WGPUDevice device) final;
  std::unique_ptr<SharedImageRepresentationSkia> ProduceSkia(
      SharedImageManager* manager,
      MemoryTypeTracker* tracker,
      scoped_refptr<SharedContextState> context_state) override;
  std::unique_ptr<SharedImageRepresentationGLTexture>
  ProduceRGBEmulationGLTexture(SharedImageManager* manager,
                               MemoryTypeTracker* tracker) override;
  void Update(std::unique_ptr<gfx::GpuFence> in_fence) override;

  // SharedImageRepresentationGLTextureImpl::Client:
  bool OnGLTextureBeginAccess(GLenum mode) override;

  // SharedImageRepresentationGLTexturePassthroughImpl::Client:
  bool OnGLTexturePassthroughBeginAccess(GLenum mode) override;

  // SharedImageRepresentationGLTextureImpl::Client:
  bool OnSkiaBeginReadAccess() override;
  bool OnSkiaBeginWriteAccess() override;

  bool IsPassthrough() const { return is_passthrough_; }

  scoped_refptr<gl::GLImage> image_;

  // If |image_bind_or_copy_needed_| is true, then either bind or copy |image_|
  // to the GL texture, and un-set |image_bind_or_copy_needed_|.
  bool BindOrCopyImageIfNeeded();
  bool image_bind_or_copy_needed_ = true;

  const SharedImageBackingGLCommon::InitializeGLTextureParams gl_params_;
  const SharedImageBackingFactoryGLTexture::UnpackStateAttribs
      gl_unpack_attribs_;
  const bool is_passthrough_;

  gles2::Texture* rgb_emulation_texture_ = nullptr;
  gles2::Texture* texture_ = nullptr;
  scoped_refptr<gles2::TexturePassthrough> passthrough_texture_;

  sk_sp<SkPromiseImageTexture> cached_promise_texture_;

  base::WeakPtrFactory<SharedImageBackingGLImage> weak_factory_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_GL_TEXTURE_INTERNAL_H_
