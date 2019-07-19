// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "components/viz/common/resources/shared_bitmap.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "gpu/ipc/common/mailbox.mojom-blink.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/canvas_color_params.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_

namespace gfx {

class GpuMemoryBuffer;

}  // namespace gfx

namespace viz {

class SingleReleaseCallback;
struct TransferableResource;

}  // namespace viz

namespace blink {

class CanvasResourceProvider;
class StaticBitmapImage;

// TODO(danakj): One day the gpu::mojom::Mailbox type should be shared with
// blink directly and we won't need to use gpu::mojom::blink::Mailbox, nor the
// conversion through WTF::Vector.
gpu::mojom::blink::MailboxPtr SharedBitmapIdToGpuMailboxPtr(
    const viz::SharedBitmapId& id);

// Generic resource interface, used for locking (RAII) and recycling pixel
// buffers of any type.
// Note that this object may be accessed across multiple threads but not
// concurrently. The caller is responsible to call Transfer on the object before
// using it on a different thread.
class PLATFORM_EXPORT CanvasResource
    : public WTF::ThreadSafeRefCounted<CanvasResource> {
 public:
  virtual ~CanvasResource();
  virtual bool IsRecycleable() const = 0;
  virtual bool IsAccelerated() const = 0;
  virtual bool SupportsAcceleratedCompositing() const = 0;
  virtual bool IsValid() const = 0;
  virtual bool NeedsReadLockFences() const { return false; }
  virtual IntSize Size() const = 0;
  virtual const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) = 0;
  virtual void Transfer() {}
  virtual const gpu::SyncToken GetSyncToken() {
    NOTREACHED();
    return gpu::SyncToken();
  }
  bool PrepareTransferableResource(viz::TransferableResource*,
                                   std::unique_ptr<viz::SingleReleaseCallback>*,
                                   MailboxSyncMode);
  void WaitSyncToken(const gpu::SyncToken&);
  virtual scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) = 0;
  virtual scoped_refptr<CanvasResource> MakeUnaccelerated() = 0;
  virtual bool OriginClean() const = 0;
  virtual void SetOriginClean(bool) = 0;
  virtual scoped_refptr<StaticBitmapImage> Bitmap() = 0;
  virtual void CopyFromTexture(GLuint source_texture,
                               GLenum format,
                               GLenum type) {
    NOTREACHED();
  }

  // Only CanvasResourceProvider and derivatives should call this.
  virtual void TakeSkImage(sk_sp<SkImage> image) = 0;

  virtual GLuint GetBackingTextureHandleForOverwrite() {
    NOTREACHED();
    return 0;
  }

  virtual GLenum TextureTarget() const {
    NOTREACHED();
    return 0;
  }

  // Called when the resource is marked lost. Losing a resource does not mean
  // that the backing memory has been destroyed, since the resource itself keeps
  // a ref on that memory.
  // It means that the consumer (commonly the compositor) can not provide a sync
  // token for the resource to be safely recycled and its the GL state may be
  // inconsistent with when the resource was given to the compositor. So it
  // should not be recycled for writing again but can be safely read from.
  virtual void NotifyResourceLost() {
    // TODO(khushalsagar): Some implementations respect the don't write again
    // policy but some don't. Fix that once shared images replace all
    // accelerated use-cases.
    Abandon();
  }

 protected:
  CanvasResource(base::WeakPtr<CanvasResourceProvider>,
                 SkFilterQuality,
                 const CanvasColorParams&);

  // Called during resource destruction if the resource is destroyed on a thread
  // other than where it was created. This implies that no context associated
  // cleanup can be done and any resources tied to the context may be leaked. As
  // such, a resource must be deleted on the owning thread and this should only
  // be called when the owning thread and its associated context was torn down
  // before this resource could be deleted.
  virtual void Abandon() { TearDown(); }

  virtual bool IsOverlayCandidate() const { return false; }
  virtual bool HasGpuMailbox() const = 0;
  virtual void TearDown() = 0;
  gpu::gles2::GLES2Interface* ContextGL() const;
  GLenum GLFilter() const;
  GrContext* GetGrContext() const;
  virtual base::WeakPtr<WebGraphicsContext3DProviderWrapper>
  ContextProviderWrapper() const {
    NOTREACHED();
    return nullptr;
  }
  bool PrepareAcceleratedTransferableResource(
      viz::TransferableResource* out_resource,
      MailboxSyncMode);
  bool PrepareUnacceleratedTransferableResource(
      viz::TransferableResource* out_resource);
  SkFilterQuality FilterQuality() const { return filter_quality_; }
  const CanvasColorParams& ColorParams() const { return color_params_; }
  void OnDestroy();
  CanvasResourceProvider* Provider() { return provider_.get(); }
  base::WeakPtr<CanvasResourceProvider> WeakProvider() { return provider_; }

 private:
  // Sync token that was provided when resource was released
  gpu::SyncToken sync_token_for_release_;
  base::WeakPtr<CanvasResourceProvider> provider_;
  SkFilterQuality filter_quality_;
  CanvasColorParams color_params_;
  blink::PlatformThreadId thread_of_origin_;
#if DCHECK_IS_ON()
  bool did_call_on_destroy_ = false;
#endif
};

// Resource type for skia Bitmaps (RAM and texture backed)
class PLATFORM_EXPORT CanvasResourceBitmap final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceBitmap> Create(
      scoped_refptr<StaticBitmapImage>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality,
      const CanvasColorParams&);
  ~CanvasResourceBitmap() override;

  // Not recyclable: Skia handles texture recycling internally and bitmaps are
  // cheap to allocate.
  bool IsRecycleable() const final { return false; }
  bool IsAccelerated() const final;
  bool SupportsAcceleratedCompositing() const override {
    return IsAccelerated();
  }
  bool IsValid() const final;
  IntSize Size() const final;
  void Transfer() final;
  scoped_refptr<StaticBitmapImage> Bitmap() final;
  bool OriginClean() const final;
  void SetOriginClean(bool value) final;
  scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) final;
  scoped_refptr<CanvasResource> MakeUnaccelerated() final;
  void TakeSkImage(sk_sp<SkImage> image) final;

 private:
  void TearDown() override;
  GLenum TextureTarget() const final;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken GetSyncToken() override;

  CanvasResourceBitmap(scoped_refptr<StaticBitmapImage>,
                       base::WeakPtr<CanvasResourceProvider>,
                       SkFilterQuality,
                       const CanvasColorParams&);

  scoped_refptr<StaticBitmapImage> image_;
};

// Resource type for GpuMemoryBuffers
class PLATFORM_EXPORT CanvasResourceGpuMemoryBuffer final
    : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceGpuMemoryBuffer> Create(
      const IntSize&,
      const CanvasColorParams&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality,
      bool is_accelerated);
  ~CanvasResourceGpuMemoryBuffer() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsAccelerated() const final { return is_accelerated_; }
  bool IsValid() const override;
  bool SupportsAcceleratedCompositing() const override { return true; }
  bool NeedsReadLockFences() const final { return true; }
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool value) final { is_origin_clean_ = value; }
  scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) final {
    NOTREACHED();
    return nullptr;
  }
  scoped_refptr<CanvasResource> MakeUnaccelerated() final {
    NOTREACHED();
    return nullptr;
  }
  void Abandon() final;
  IntSize Size() const final;
  void TakeSkImage(sk_sp<SkImage> image) final;
  void CopyFromTexture(GLuint source_texture,
                       GLenum format,
                       GLenum type) override;
  scoped_refptr<StaticBitmapImage> Bitmap() override;
  GLuint GetBackingTextureHandleForOverwrite() final;

 private:
  void TearDown() override;
  GLenum TextureTarget() const final;
  bool IsOverlayCandidate() const final { return true; }
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken GetSyncToken() override;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  void WillPaint();
  void DidPaint();

  CanvasResourceGpuMemoryBuffer(
      const IntSize&,
      const CanvasColorParams&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality,
      bool is_accelerated);

  gpu::Mailbox gpu_mailbox_;
  gpu::SyncToken sync_token_;
  bool mailbox_needs_new_sync_token_ = false;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  std::unique_ptr<gfx::GpuMemoryBuffer> gpu_memory_buffer_;
  void* buffer_base_address_ = nullptr;
  sk_sp<SkSurface> surface_;
  GLuint texture_id_ = 0;
  MailboxSyncMode mailbox_sync_mode_ = kVerifiedSyncToken;
  bool is_accelerated_;
  bool is_origin_clean_ = true;

  // GL_TEXTURE_2D view of |gpu_memory_buffer_| for CopyFromTexture(); only used
  // if TextureTarget() is GL_TEXTURE_EXTERNAL_OES.
  GLuint texture_2d_id_for_copy_ = 0;
};

// Resource type for SharedBitmaps
class PLATFORM_EXPORT CanvasResourceSharedBitmap final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceSharedBitmap> Create(
      const IntSize&,
      const CanvasColorParams&,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality);
  ~CanvasResourceSharedBitmap() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsAccelerated() const final { return false; }
  bool IsValid() const final;
  bool SupportsAcceleratedCompositing() const final { return false; }
  bool NeedsReadLockFences() const final { return false; }
  scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) final {
    NOTREACHED();
    return nullptr;
  }
  scoped_refptr<CanvasResource> MakeUnaccelerated() final {
    NOTREACHED();
    return nullptr;
  }
  void Abandon() final;
  IntSize Size() const final;
  void TakeSkImage(sk_sp<SkImage> image) final;
  void CopyFromTexture(GLuint source_texture,
                       GLenum format,
                       GLenum type) override {
    NOTREACHED();
  }
  scoped_refptr<StaticBitmapImage> Bitmap() final;
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool flag) final { is_origin_clean_ = flag; }

 private:
  void TearDown() override;
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  bool HasGpuMailbox() const override;

  CanvasResourceSharedBitmap(const IntSize&,
                             const CanvasColorParams&,
                             base::WeakPtr<CanvasResourceProvider>,
                             SkFilterQuality);

  viz::SharedBitmapId shared_bitmap_id_;
  base::WritableSharedMemoryMapping shared_mapping_;
  IntSize size_;
  bool is_origin_clean_ = true;
};

// Resource type for SharedImage
class PLATFORM_EXPORT CanvasResourceSharedImage final : public CanvasResource {
 public:
  static scoped_refptr<CanvasResourceSharedImage> Create(
      const IntSize&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality,
      const CanvasColorParams&,
      bool is_overlay_candidate,
      bool is_origin_top_left);
  ~CanvasResourceSharedImage() override;

  bool IsRecycleable() const final { return true; }
  bool IsAccelerated() const final { return true; }
  bool SupportsAcceleratedCompositing() const override { return true; }
  bool IsValid() const final;
  IntSize Size() const final { return size_; }
  scoped_refptr<StaticBitmapImage> Bitmap() final;
  void Transfer() final;

  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool value) final { is_origin_clean_ = value; }
  scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) final {
    NOTREACHED();
    return nullptr;
  }
  scoped_refptr<CanvasResource> MakeUnaccelerated() final {
    NOTREACHED();
    return nullptr;
  }
  void TakeSkImage(sk_sp<SkImage> image) final { NOTREACHED(); }
  void NotifyResourceLost() final;

  GLuint GetTextureIdForBackendTexture() const {
    return owning_thread_data().texture_id;
  }
  void WillDraw();
  bool is_cross_thread() const {
    return base::PlatformThread::CurrentId() != owning_thread_id_;
  }
  bool has_read_access() const {
    return owning_thread_data().bitmap_image_read_refs > 0u;
  }
  bool is_lost() const { return owning_thread_data().is_lost; }

 private:
  // These members are either only accessed on the owning thread, or are only
  // updated on the owning thread and then are read on a different thread.
  // We ensure to correctly update their state in Transfer, which is called
  // before a resource is used on a different thread.
  struct OwningThreadData {
    bool mailbox_needs_new_sync_token = true;
    gpu::Mailbox shared_image_mailbox;
    gpu::SyncToken sync_token;
    bool needs_gl_filter_reset = true;
    size_t bitmap_image_read_refs = 0u;
    GLuint texture_id = 0u;
    MailboxSyncMode mailbox_sync_mode = kVerifiedSyncToken;
    bool is_lost = false;
  };

  static void OnBitmapImageDestroyed(
      scoped_refptr<CanvasResourceSharedImage> resource,
      bool has_read_ref_on_texture,
      const gpu::SyncToken& sync_token,
      bool is_lost);

  void TearDown() override;
  void Abandon() override;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  GLenum TextureTarget() const final;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken GetSyncToken() override;
  bool IsOverlayCandidate() const final { return is_overlay_candidate_; }

  CanvasResourceSharedImage(const IntSize&,
                            base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                            base::WeakPtr<CanvasResourceProvider>,
                            SkFilterQuality,
                            const CanvasColorParams&,
                            bool is_overlay_candidate,
                            bool is_origin_top_left);
  void SetGLFilterIfNeeded();

  OwningThreadData& owning_thread_data() {
    DCHECK_EQ(base::PlatformThread::CurrentId(), owning_thread_id_);
    return owning_thread_data_;
  }
  const OwningThreadData& owning_thread_data() const {
    DCHECK_EQ(base::PlatformThread::CurrentId(), owning_thread_id_);
    return owning_thread_data_;
  }

  // Can be read on any thread but updated only on the owning thread.
  const gpu::Mailbox& mailbox() const {
    return owning_thread_data_.shared_image_mailbox;
  }
  bool mailbox_needs_new_sync_token() const {
    return owning_thread_data_.mailbox_needs_new_sync_token;
  }
  const gpu::SyncToken& sync_token() const {
    return owning_thread_data_.sync_token;
  }

  // This should only be de-referenced on the owning thread but may be copied
  // on a different thread.
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;

  // This can be accessed on any thread, irrespective of whether there are
  // active readers or not.
  bool is_origin_clean_ = true;

  // Accessed on any thread.
  const bool is_overlay_candidate_;
  const IntSize size_;
  const bool is_origin_top_left_;
  const GLenum texture_target_;
  const base::PlatformThreadId owning_thread_id_;
  const scoped_refptr<base::SingleThreadTaskRunner> owning_thread_task_runner_;

  OwningThreadData owning_thread_data_;
};

// Resource type for a given opaque external resource described on construction
// via a Mailbox; this CanvasResource IsAccelerated() by definition.
class PLATFORM_EXPORT ExternalCanvasResource final : public CanvasResource {
 public:
  static scoped_refptr<ExternalCanvasResource> Create(
      const gpu::Mailbox&,
      const IntSize&,
      GLenum texture_target,
      const CanvasColorParams&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      base::WeakPtr<CanvasResourceProvider>,
      SkFilterQuality);
  ~ExternalCanvasResource() override;
  bool IsRecycleable() const final { return IsValid(); }
  bool IsAccelerated() const final { return true; }
  bool IsValid() const override;
  bool SupportsAcceleratedCompositing() const override { return true; }
  bool NeedsReadLockFences() const final { return false; }
  bool OriginClean() const final { return is_origin_clean_; }
  void SetOriginClean(bool value) final { is_origin_clean_ = value; }
  scoped_refptr<CanvasResource> MakeAccelerated(
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>) final {
    NOTREACHED();
    return nullptr;
  }
  scoped_refptr<CanvasResource> MakeUnaccelerated() final {
    NOTREACHED();
    return nullptr;
  }
  void Abandon() final;
  IntSize Size() const final { return size_; }
  void TakeSkImage(sk_sp<SkImage> image) final;

  scoped_refptr<StaticBitmapImage> Bitmap() override;

 private:
  void TearDown() override;
  GLenum TextureTarget() const final { return texture_target_; }
  bool IsOverlayCandidate() const final { return true; }
  const gpu::Mailbox& GetOrCreateGpuMailbox(MailboxSyncMode) override;
  bool HasGpuMailbox() const override;
  const gpu::SyncToken GetSyncToken() override;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper()
      const override;

  ExternalCanvasResource(const gpu::Mailbox&,
                         const IntSize&,
                         GLenum texture_target,
                         const CanvasColorParams&,
                         base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                         base::WeakPtr<CanvasResourceProvider>,
                         SkFilterQuality);

  const base::WeakPtr<WebGraphicsContext3DProviderWrapper>
      context_provider_wrapper_;
  const IntSize size_;
  const GLenum texture_target_;
  gpu::Mailbox mailbox_;
  gpu::SyncToken sync_token_;

  bool is_origin_clean_ = true;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_H_
