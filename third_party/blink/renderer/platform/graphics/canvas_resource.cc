// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"

#include <utility>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/viz/common/resources/bitmap_allocation.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/single_release_callback.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_dispatcher.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/color_space.h"

namespace blink {

// TODO(danakj): One day the gpu::mojom::Mailbox type should be shared with
// blink directly and we won't need to use gpu::mojom::blink::Mailbox, nor the
// conversion through WTF::Vector.
gpu::mojom::blink::MailboxPtr SharedBitmapIdToGpuMailboxPtr(
    const viz::SharedBitmapId& id) {
  WTF::Vector<int8_t> name(GL_MAILBOX_SIZE_CHROMIUM);
  for (int i = 0; i < GL_MAILBOX_SIZE_CHROMIUM; ++i)
    name[i] = id.name[i];
  return {base::in_place, name};
}

CanvasResource::CanvasResource(base::WeakPtr<CanvasResourceProvider> provider,
                               SkFilterQuality filter_quality,
                               const CanvasColorParams& color_params)
    : provider_(std::move(provider)),
      filter_quality_(filter_quality),
      color_params_(color_params) {
  thread_of_origin_ = Thread::Current()->ThreadId();
}

CanvasResource::~CanvasResource() {
#if DCHECK_IS_ON()
  DCHECK(did_call_on_destroy_);
#endif
}

void CanvasResource::OnDestroy() {
  if (thread_of_origin_ != Thread::Current()->ThreadId()) {
    // Destroyed on wrong thread. This can happen when the thread of origin was
    // torn down, in which case the GPU context owning any underlying resources
    // no longer exists.
    Abandon();
  } else {
    TearDown();
  }
#if DCHECK_IS_ON()
  did_call_on_destroy_ = true;
#endif
}

gpu::gles2::GLES2Interface* CanvasResource::ContextGL() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->ContextGL();
}

void CanvasResource::WaitSyncToken(const gpu::SyncToken& sync_token) {
  if (sync_token.HasData()) {
    auto* gl = ContextGL();
    if (gl)
      gl->WaitSyncTokenCHROMIUM(sync_token.GetConstData());
  }
}

static void ReleaseFrameResources(
    base::WeakPtr<CanvasResourceProvider> resource_provider,
    scoped_refptr<CanvasResource> resource,
    const gpu::SyncToken& sync_token,
    bool lost_resource) {
  resource->WaitSyncToken(sync_token);

  // TODO(khushalsagar): If multiple readers had access to this resource, losing
  // it once should make sure subsequent releases don't try to recycle this
  // resource.
  // Also what about single buffered canvas?
  if (lost_resource)
    resource->NotifyResourceLost();
  if (resource_provider && !lost_resource && resource->IsRecycleable())
    resource_provider->RecycleResource(std::move(resource));
}

bool CanvasResource::PrepareTransferableResource(
    viz::TransferableResource* out_resource,
    std::unique_ptr<viz::SingleReleaseCallback>* out_callback,
    MailboxSyncMode sync_mode) {
  DCHECK(IsValid());

  DCHECK(out_callback);
  auto func = WTF::Bind(&ReleaseFrameResources, provider_,
                        WTF::Passed(base::WrapRefCounted(this)));
  *out_callback = viz::SingleReleaseCallback::Create(std::move(func));

  if (!out_resource)
    return true;
  if (SupportsAcceleratedCompositing())
    return PrepareAcceleratedTransferableResource(out_resource, sync_mode);
  return PrepareUnacceleratedTransferableResource(out_resource);
}

bool CanvasResource::PrepareAcceleratedTransferableResource(
    viz::TransferableResource* out_resource,
    MailboxSyncMode sync_mode) {
  TRACE_EVENT0("blink",
               "CanvasResource::PrepareAcceleratedTransferableResource");
  // Gpu compositing is a prerequisite for compositing an accelerated resource
  DCHECK(SharedGpuContext::IsGpuCompositingEnabled());
  auto* gl = ContextGL();
  if (!gl)
    return false;
  const gpu::Mailbox& mailbox = GetOrCreateGpuMailbox(sync_mode);
  if (mailbox.IsZero())
    return false;

  *out_resource = viz::TransferableResource::MakeGL(
      mailbox, GLFilter(), TextureTarget(), GetSyncToken(), gfx::Size(Size()),
      IsOverlayCandidate());

  out_resource->color_space = color_params_.GetSamplerGfxColorSpace();
  out_resource->format = color_params_.TransferableResourceFormat();
  out_resource->read_lock_fences_enabled = NeedsReadLockFences();

  return true;
}

bool CanvasResource::PrepareUnacceleratedTransferableResource(
    viz::TransferableResource* out_resource) {
  TRACE_EVENT0("blink",
               "CanvasResource::PrepareUnacceleratedTransferableResource");
  const gpu::Mailbox& mailbox = GetOrCreateGpuMailbox(kVerifiedSyncToken);
  if (mailbox.IsZero())
    return false;

  // For software compositing, the display compositor assumes an N32 format for
  // the resource type and completely ignores the format set on the
  // TransferableResource. Clients are expected to render in N32 format but use
  // RGBA as the tagged format on resources.
  *out_resource = viz::TransferableResource::MakeSoftware(
      mailbox, gfx::Size(Size()), viz::RGBA_8888);

  out_resource->color_space = color_params_.GetSamplerGfxColorSpace();

  return true;
}

GrContext* CanvasResource::GetGrContext() const {
  if (!ContextProviderWrapper())
    return nullptr;
  return ContextProviderWrapper()->ContextProvider()->GetGrContext();
}

GLenum CanvasResource::GLFilter() const {
  return filter_quality_ == kNone_SkFilterQuality ? GL_NEAREST : GL_LINEAR;
}

// CanvasResourceBitmap
//==============================================================================

CanvasResourceBitmap::CanvasResourceBitmap(
    scoped_refptr<StaticBitmapImage> image,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      image_(std::move(image)) {}

CanvasResourceBitmap::~CanvasResourceBitmap() {
  OnDestroy();
}

scoped_refptr<CanvasResourceBitmap> CanvasResourceBitmap::Create(
    scoped_refptr<StaticBitmapImage> image,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params) {
  auto resource = AdoptRef(new CanvasResourceBitmap(
      std::move(image), std::move(provider), filter_quality, color_params));
  return resource->IsValid() ? resource : nullptr;
}

bool CanvasResourceBitmap::IsValid() const {
  return image_ ? image_->IsValid() : false;
}

bool CanvasResourceBitmap::IsAccelerated() const {
  return image_->IsTextureBacked();
}

scoped_refptr<CanvasResource> CanvasResourceBitmap::MakeAccelerated(
    base::WeakPtr<WebGraphicsContext3DProviderWrapper>
        context_provider_wrapper) {
  if (IsAccelerated())
    return base::WrapRefCounted(this);

  TRACE_EVENT0("blink", "CanvasResourceBitmap::MakeAccelerated");

  if (!context_provider_wrapper)
    return nullptr;
  scoped_refptr<StaticBitmapImage> accelerated_image =
      image_->MakeAccelerated(context_provider_wrapper);
  // passing nullptr for the resource provider argument creates an orphan
  // CanvasResource, which implies that it internal resources will not be
  // recycled.
  scoped_refptr<CanvasResource> accelerated_resource =
      Create(accelerated_image, nullptr, FilterQuality(), ColorParams());
  if (!accelerated_resource)
    return nullptr;
  return accelerated_resource;
}

scoped_refptr<CanvasResource> CanvasResourceBitmap::MakeUnaccelerated() {
  if (!IsAccelerated())
    return base::WrapRefCounted(this);

  TRACE_EVENT0("blink", "CanvasResourceBitmap::MakeUnaccelerated");

  scoped_refptr<StaticBitmapImage> unaccelerated_image =
      image_->MakeUnaccelerated();
  // passing nullptr for the resource provider argument creates an orphan
  // CanvasResource, which implies that it internal resources will not be
  // recycled.
  scoped_refptr<CanvasResource> unaccelerated_resource =
      Create(unaccelerated_image, nullptr, FilterQuality(), ColorParams());
  return unaccelerated_resource;
}

void CanvasResourceBitmap::TearDown() {
  image_ = nullptr;
}

IntSize CanvasResourceBitmap::Size() const {
  if (!image_)
    return IntSize(0, 0);
  return IntSize(image_->width(), image_->height());
}

GLenum CanvasResourceBitmap::TextureTarget() const {
  return GL_TEXTURE_2D;
}

scoped_refptr<StaticBitmapImage> CanvasResourceBitmap::Bitmap() {
  return image_;
}

const gpu::Mailbox& CanvasResourceBitmap::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  DCHECK(image_);  // Calling code should check IsValid() before calling this.
  image_->EnsureMailbox(sync_mode, GLFilter());
  return image_->GetMailbox();
}

bool CanvasResourceBitmap::HasGpuMailbox() const {
  return image_ && image_->HasMailbox();
}

const gpu::SyncToken CanvasResourceBitmap::GetSyncToken() {
  DCHECK(image_);  // Calling code should check IsValid() before calling this.
  return image_->GetSyncToken();
}

void CanvasResourceBitmap::Transfer() {
  DCHECK(image_);  // Calling code should check IsValid() before calling this.
  return image_->Transfer();
}

bool CanvasResourceBitmap::OriginClean() const {
  DCHECK(image_);
  return image_->OriginClean();
}

void CanvasResourceBitmap::SetOriginClean(bool value) {
  DCHECK(image_);
  image_->SetOriginClean(value);
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResourceBitmap::ContextProviderWrapper() const {
  if (!image_)
    return nullptr;
  return image_->ContextProviderWrapper();
}

void CanvasResourceBitmap::TakeSkImage(sk_sp<SkImage> image) {
  DCHECK(IsAccelerated() == image->isTextureBacked());
  image_ =
      StaticBitmapImage::Create(std::move(image), ContextProviderWrapper());
}

// CanvasResourceGpuMemoryBuffer
//==============================================================================

CanvasResourceGpuMemoryBuffer::CanvasResourceGpuMemoryBuffer(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    bool is_accelerated)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      is_accelerated_(is_accelerated) {
  if (!context_provider_wrapper_)
    return;
  auto* gl = context_provider_wrapper_->ContextProvider()->ContextGL();
  auto* gr = context_provider_wrapper_->ContextProvider()->GetGrContext();
  if (!gl || !gr)
    return;

  const gfx::BufferUsage buffer_usage =
      is_accelerated ? gfx::BufferUsage::SCANOUT
                     : gfx::BufferUsage::SCANOUT_CPU_READ_WRITE;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager =
      Platform::Current()->GetGpuMemoryBufferManager();
  if (!gpu_memory_buffer_manager)
    return;
  gpu_memory_buffer_ = gpu_memory_buffer_manager->CreateGpuMemoryBuffer(
      gfx::Size(size.Width(), size.Height()), ColorParams().GetBufferFormat(),
      buffer_usage, gpu::kNullSurfaceHandle);
  if (!gpu_memory_buffer_)
    return;

  gpu_memory_buffer_->SetColorSpace(color_params.GetStorageGfxColorSpace());

  const GLuint image_id = gl->CreateImageCHROMIUM(
      gpu_memory_buffer_->AsClientBuffer(), size.Width(), size.Height(),
      ColorParams().GLUnsizedInternalFormat());
  if (!image_id) {
    gpu_memory_buffer_ = nullptr;
    return;
  }
  gl->GenTextures(1, &texture_id_);
  const GLenum target = TextureTarget();
  gl->BindTexture(target, texture_id_);
  gl->BindTexImage2DCHROMIUM(target, image_id);
  gl->DestroyImageCHROMIUM(image_id);

  if (is_accelerated_ && target == GL_TEXTURE_EXTERNAL_OES) {
    // We can't CopyTextureCHROMIUM() into a GL_TEXTURE_EXTERNAL_OES; create
    // another image and bind a GL_TEXTURE_2D texture to it.
    const GLuint image_2d_id_for_copy = gl->CreateImageCHROMIUM(
        gpu_memory_buffer_->AsClientBuffer(), size.Width(), size.Height(),
        ColorParams().GLUnsizedInternalFormat());

    gl->GenTextures(1, &texture_2d_id_for_copy_);
    gl->BindTexture(GL_TEXTURE_2D, texture_2d_id_for_copy_);
    gl->BindTexImage2DCHROMIUM(GL_TEXTURE_2D, image_2d_id_for_copy);

    gl->DestroyImageCHROMIUM(image_2d_id_for_copy);
  }
}

CanvasResourceGpuMemoryBuffer::~CanvasResourceGpuMemoryBuffer() {
  OnDestroy();
}

bool CanvasResourceGpuMemoryBuffer::IsValid() const {
  return !!context_provider_wrapper_ && gpu_memory_buffer_;
}

GLenum CanvasResourceGpuMemoryBuffer::TextureTarget() const {
  return gpu::GetPlatformSpecificTextureTarget();
}

IntSize CanvasResourceGpuMemoryBuffer::Size() const {
  return IntSize(gpu_memory_buffer_->GetSize());
}

scoped_refptr<CanvasResourceGpuMemoryBuffer>
CanvasResourceGpuMemoryBuffer::Create(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    bool is_accelerated) {
  TRACE_EVENT0("blink", "CanvasResourceGpuMemoryBuffer::Create");
  auto resource = AdoptRef(new CanvasResourceGpuMemoryBuffer(
      size, color_params, std::move(context_provider_wrapper),
      std::move(provider), filter_quality, is_accelerated));
  return resource->IsValid() ? resource : nullptr;
}

void CanvasResourceGpuMemoryBuffer::TearDown() {
  // Unref should result in destruction.
  DCHECK(!surface_ || surface_->unique());

  surface_ = nullptr;
  if (context_provider_wrapper_) {
    auto* gl = ContextGL();
    if (gl && texture_id_)
      gl->DeleteTextures(1, &texture_id_);
    if (gl && texture_2d_id_for_copy_)
      gl->DeleteTextures(1, &texture_2d_id_for_copy_);
  }
  texture_id_ = 0;
  texture_2d_id_for_copy_ = 0;
  gpu_memory_buffer_ = nullptr;
}

void CanvasResourceGpuMemoryBuffer::Abandon() {
  surface_ = nullptr;
  texture_id_ = 0;
  texture_2d_id_for_copy_ = 0;
  gpu_memory_buffer_ = nullptr;
}

const gpu::Mailbox& CanvasResourceGpuMemoryBuffer::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  auto* gl = ContextGL();
  if (gpu_mailbox_.IsZero() && gl) {
    gl->ProduceTextureDirectCHROMIUM(texture_id_, gpu_mailbox_.name);
    mailbox_needs_new_sync_token_ = true;
    mailbox_sync_mode_ = sync_mode;
  }
  return gpu_mailbox_;
}

bool CanvasResourceGpuMemoryBuffer::HasGpuMailbox() const {
  return !gpu_mailbox_.IsZero();
}

GLuint CanvasResourceGpuMemoryBuffer::GetBackingTextureHandleForOverwrite() {
  return texture_2d_id_for_copy_ ? texture_2d_id_for_copy_ : texture_id_;
}

const gpu::SyncToken CanvasResourceGpuMemoryBuffer::GetSyncToken() {
  if (!mailbox_needs_new_sync_token_)
    return sync_token_;
  auto* gl = ContextGL();
  if (!gl)
    return sync_token_;
  mailbox_needs_new_sync_token_ = false;
  if (mailbox_sync_mode_ == kVerifiedSyncToken)
    gl->GenSyncTokenCHROMIUM(sync_token_.GetData());
  else
    gl->GenUnverifiedSyncTokenCHROMIUM(sync_token_.GetData());

  return sync_token_;
}

void CanvasResourceGpuMemoryBuffer::CopyFromTexture(GLuint source_texture,
                                                    GLenum format,
                                                    GLenum type) {
  DCHECK(is_accelerated_);
  if (!IsValid())
    return;

  TRACE_EVENT0("blink", "CanvasResourceGpuMemoryBuffer::CopyFromTexture");
  GLenum target = TextureTarget();
  GLuint texture_id = texture_id_;

  if (texture_2d_id_for_copy_) {
    // We can't CopyTextureCHROMIUM() into a GL_TEXTURE_EXTERNAL_OES; use
    // instead GL_TEXTURE_2D for the CopyTextureChromium().
    target = GL_TEXTURE_2D;
    texture_id = texture_2d_id_for_copy_;
  }

  ContextGL()->CopyTextureCHROMIUM(
      source_texture, 0 /*sourceLevel*/, target, texture_id, 0 /*destLevel*/,
      format, type, false /*unpackFlipY*/, false /*unpackPremultiplyAlpha*/,
      false /*unpackUnmultiplyAlpha*/);

  mailbox_needs_new_sync_token_ = true;
}

void CanvasResourceGpuMemoryBuffer::TakeSkImage(sk_sp<SkImage> image) {
  WillPaint();
  if (!surface_)
    return;
  surface_->getCanvas()->drawImage(image, 0, 0);
  DidPaint();
}

void CanvasResourceGpuMemoryBuffer::WillPaint() {
  if (!IsValid()) {
    surface_ = nullptr;
    return;
  }

  TRACE_EVENT1("blink", "CanvasResourceGpuMemoryBuffer::WillPaint",
               "accelerated", is_accelerated_);

  if (is_accelerated_) {
    if (!surface_) {  // When accelerated it is okay to re-use previous
                      // SkSurface
      GrGLTextureInfo texture_info;
      texture_info.fTarget = TextureTarget();
      texture_info.fID = texture_id_;
      texture_info.fFormat =
          ColorParams().GLSizedInternalFormat();  // unsized format
      GrBackendTexture backend_texture(Size().Width(), Size().Height(),
                                       GrMipMapped::kNo, texture_info);
      constexpr int sample_count = 0;
      surface_ = SkSurface::MakeFromBackendTextureAsRenderTarget(
          GetGrContext(), backend_texture, kTopLeft_GrSurfaceOrigin,
          sample_count, ColorParams().GetSkColorType(),
          ColorParams().GetSkColorSpace(), nullptr /*surface props*/);
    }
  } else {
    gpu_memory_buffer_->Map();
    void* buffer_base_address = gpu_memory_buffer_->memory(0);
    if (!surface_ || buffer_base_address != buffer_base_address_) {
      buffer_base_address_ = buffer_base_address;
      SkImageInfo image_info = SkImageInfo::Make(
          Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
          ColorParams().GetSkAlphaType(), ColorParams().GetSkColorSpace());
      surface_ = SkSurface::MakeRasterDirect(image_info, buffer_base_address_,
                                             gpu_memory_buffer_->stride(0));
    }
  }

  DCHECK(surface_);
}

void CanvasResourceGpuMemoryBuffer::DidPaint() {
  TRACE_EVENT1("blink", "CanvasResourceGpuMemoryBuffer::DidPaint",
               "accelerated", is_accelerated_);

  if (is_accelerated_) {
    auto* gr = context_provider_wrapper_->ContextProvider()->GetGrContext();
    if (gr)
      gr->flush();
    mailbox_needs_new_sync_token_ = true;
  } else {
    gpu_memory_buffer_->Unmap();
  }
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResourceGpuMemoryBuffer::ContextProviderWrapper() const {
  return context_provider_wrapper_;
}

scoped_refptr<StaticBitmapImage> CanvasResourceGpuMemoryBuffer::Bitmap() {
  WillPaint();
  scoped_refptr<StaticBitmapImage> bitmap = StaticBitmapImage::Create(
      surface_->makeImageSnapshot(), ContextProviderWrapper());
  DidPaint();
  bitmap->SetOriginClean(is_origin_clean_);
  return bitmap;
}

// CanvasResourceSharedBitmap
//==============================================================================

CanvasResourceSharedBitmap::CanvasResourceSharedBitmap(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      size_(size) {
  // Software compositing lazily uses RGBA_8888 as the resource format
  // everywhere but the content is expected to be rendered in N32 format.
  base::MappedReadOnlyRegion shm = viz::bitmap_allocation::AllocateSharedBitmap(
      gfx::Size(Size()), viz::RGBA_8888);

  if (!shm.IsValid())
    return;

  shared_mapping_ = std::move(shm.mapping);
  shared_bitmap_id_ = viz::SharedBitmap::GenerateId();

  CanvasResourceDispatcher* resource_dispatcher =
      Provider() ? Provider()->ResourceDispatcher() : nullptr;
  if (resource_dispatcher) {
    resource_dispatcher->DidAllocateSharedBitmap(
        std::move(shm.region),
        SharedBitmapIdToGpuMailboxPtr(shared_bitmap_id_));
  }
}

CanvasResourceSharedBitmap::~CanvasResourceSharedBitmap() {
  OnDestroy();
}

bool CanvasResourceSharedBitmap::IsValid() const {
  return shared_mapping_.IsValid();
}

IntSize CanvasResourceSharedBitmap::Size() const {
  return size_;
}

scoped_refptr<StaticBitmapImage> CanvasResourceSharedBitmap::Bitmap() {
  if (!IsValid())
    return nullptr;
  // Construct an SkImage that references the shared memory buffer.
  // The release callback holds a reference to |this| to ensure that the
  // canvas resource that owns the shared memory stays alive at least until
  // the SkImage is destroyed.
  SkImageInfo image_info = SkImageInfo::Make(
      Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
      ColorParams().GetSkAlphaType(), ColorParams().GetSkColorSpace());
  SkPixmap pixmap(image_info, shared_mapping_.memory(),
                  image_info.minRowBytes());
  this->AddRef();
  sk_sp<SkImage> sk_image = SkImage::MakeFromRaster(
      pixmap,
      [](const void*, SkImage::ReleaseContext resource_to_unref) {
        static_cast<CanvasResourceSharedBitmap*>(resource_to_unref)->Release();
      },
      this);
  auto image = StaticBitmapImage::Create(sk_image);
  image->SetOriginClean(is_origin_clean_);
  return image;
}

scoped_refptr<CanvasResourceSharedBitmap> CanvasResourceSharedBitmap::Create(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality) {
  auto resource = AdoptRef(new CanvasResourceSharedBitmap(
      size, color_params, std::move(provider), filter_quality));
  return resource->IsValid() ? resource : nullptr;
}

void CanvasResourceSharedBitmap::TearDown() {
  CanvasResourceDispatcher* resource_dispatcher =
      Provider() ? Provider()->ResourceDispatcher() : nullptr;
  if (resource_dispatcher && !shared_bitmap_id_.IsZero()) {
    resource_dispatcher->DidDeleteSharedBitmap(
        SharedBitmapIdToGpuMailboxPtr(shared_bitmap_id_));
  }
  shared_mapping_ = {};
}

void CanvasResourceSharedBitmap::Abandon() {
  shared_mapping_ = {};
}

const gpu::Mailbox& CanvasResourceSharedBitmap::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  return shared_bitmap_id_;
}

bool CanvasResourceSharedBitmap::HasGpuMailbox() const {
  return !shared_bitmap_id_.IsZero();
}

void CanvasResourceSharedBitmap::TakeSkImage(sk_sp<SkImage> image) {
  SkImageInfo image_info = SkImageInfo::Make(
      Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
      ColorParams().GetSkAlphaType(),
      ColorParams().GetSkColorSpaceForSkSurfaces());

  bool read_pixels_successful = image->readPixels(
      image_info, shared_mapping_.memory(), image_info.minRowBytes(), 0, 0);
  DCHECK(read_pixels_successful);
}

// CanvasResourceSharedImage
//==============================================================================

CanvasResourceSharedImage::CanvasResourceSharedImage(
    const IntSize& size,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params,
    bool is_overlay_candidate,
    bool is_origin_top_left)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      is_overlay_candidate_(is_overlay_candidate),
      size_(size),
      is_origin_top_left_(is_origin_top_left),
      texture_target_(
          is_overlay_candidate_
              ? gpu::GetBufferTextureTarget(
                    gfx::BufferUsage::SCANOUT,
                    BufferFormat(ColorParams().TransferableResourceFormat()),
                    context_provider_wrapper_->ContextProvider()
                        ->GetCapabilities())
              : GL_TEXTURE_2D),
      owning_thread_id_(base::PlatformThread::CurrentId()),
      owning_thread_task_runner_(Thread::Current()->GetTaskRunner()) {
  if (!context_provider_wrapper_)
    return;

  auto* shared_image_interface =
      context_provider_wrapper_->ContextProvider()->SharedImageInterface();
  DCHECK(shared_image_interface);

  uint32_t flags = gpu::SHARED_IMAGE_USAGE_DISPLAY |
                   gpu::SHARED_IMAGE_USAGE_GLES2 |
                   gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT;
  if (is_overlay_candidate_)
    flags |= gpu::SHARED_IMAGE_USAGE_SCANOUT;

  auto shared_image_mailbox = shared_image_interface->CreateSharedImage(
      ColorParams().TransferableResourceFormat(), gfx::Size(size),
      ColorParams().GetStorageGfxColorSpace(), flags);

  // Wait for the mailbox to be ready to be used.
  WaitSyncToken(shared_image_interface->GenUnverifiedSyncToken());

  auto* gl = ContextGL();
  DCHECK(gl);
  owning_thread_data().shared_image_mailbox = shared_image_mailbox;
  owning_thread_data().texture_id =
      gl->CreateAndTexStorage2DSharedImageCHROMIUM(shared_image_mailbox.name);
}

scoped_refptr<CanvasResourceSharedImage> CanvasResourceSharedImage::Create(
    const IntSize& size,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality,
    const CanvasColorParams& color_params,
    bool is_overlay_candidate,
    bool is_origin_top_left) {
  TRACE_EVENT0("blink", "CanvasResourceSharedImage::Create");
  auto resource = base::AdoptRef(new CanvasResourceSharedImage(
      size, std::move(context_provider_wrapper), std::move(provider),
      filter_quality, color_params, is_overlay_candidate, is_origin_top_left));
  return resource->IsValid() ? resource : nullptr;
}

bool CanvasResourceSharedImage::IsValid() const {
  return !mailbox().IsZero();
}

CanvasResourceSharedImage::~CanvasResourceSharedImage() {
  OnDestroy();
}

GLenum CanvasResourceSharedImage::TextureTarget() const {
  return texture_target_;
}

void CanvasResourceSharedImage::TearDown() {
  DCHECK(!is_cross_thread());

  if (ContextProviderWrapper()) {
    auto* gl = ContextGL();
    auto* shared_image_interface =
        ContextProviderWrapper()->ContextProvider()->SharedImageInterface();
    if (gl && shared_image_interface) {
      gpu::SyncToken shared_image_sync_token;
      gl->GenUnverifiedSyncTokenCHROMIUM(shared_image_sync_token.GetData());
      shared_image_interface->DestroySharedImage(shared_image_sync_token,
                                                 mailbox());
    }
    if (gl && owning_thread_data().texture_id)
      gl->DeleteTextures(1, &owning_thread_data().texture_id);
  }
  owning_thread_data().texture_id = 0u;
}

void CanvasResourceSharedImage::Abandon() {
  if (auto context_provider = SharedGpuContext::ContextProviderWrapper()) {
    auto* sii = context_provider->ContextProvider()->SharedImageInterface();
    if (sii)
      sii->DestroySharedImage(gpu::SyncToken(), mailbox());
  }
}

void CanvasResourceSharedImage::WillDraw() {
  DCHECK(!is_cross_thread())
      << "Write access is only allowed on the owning thread";
  // If skia is accessing the resource, it can modify the texture's filter
  // params. Make sure to set them to the desired value before sending the
  // resource to the display compositor.
  owning_thread_data().needs_gl_filter_reset = true;
  owning_thread_data().mailbox_needs_new_sync_token = true;
}

// static
void CanvasResourceSharedImage::OnBitmapImageDestroyed(
    scoped_refptr<CanvasResourceSharedImage> resource,
    bool has_read_ref_on_texture,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  if (resource->is_cross_thread()) {
    auto& task_runner = *resource->owning_thread_task_runner_;
    PostCrossThreadTask(
        task_runner, FROM_HERE,
        CrossThreadBindOnce(&CanvasResourceSharedImage::OnBitmapImageDestroyed,
                            std::move(resource), has_read_ref_on_texture,
                            sync_token, is_lost));
    return;
  }

  if (has_read_ref_on_texture) {
    DCHECK_GT(resource->owning_thread_data().bitmap_image_read_refs, 0u);

    resource->owning_thread_data().bitmap_image_read_refs--;
    if (resource->owning_thread_data().bitmap_image_read_refs == 0u &&
        resource->ContextGL()) {
      resource->ContextGL()->EndSharedImageAccessDirectCHROMIUM(
          resource->owning_thread_data().texture_id);
    }
  }

  // The StaticBitmapImage is used for readbacks which may modify the texture
  // params. Note that this is racy, since the modification and resetting of the
  // param is not atomic so the display may draw with incorrect params, but its
  // a good enough fix for now.
  resource->owning_thread_data().needs_gl_filter_reset = true;
  auto weak_provider = resource->WeakProvider();
  ReleaseFrameResources(std::move(weak_provider), std::move(resource),
                        sync_token, is_lost);
}

void CanvasResourceSharedImage::Transfer() {
  if (is_cross_thread() || !ContextProviderWrapper())
    return;

  // Initialize GLFilter first so that the generated sync token includes this
  // update.
  SetGLFilterIfNeeded();

  // TODO(khushalsagar): This is for consistency with MailboxTextureHolder
  // transfer path. Its unclear why the verification can not be deferred until
  // the resource needs to be transferred cross-process.
  owning_thread_data().mailbox_sync_mode = kVerifiedSyncToken;
  GetSyncToken();
}

scoped_refptr<StaticBitmapImage> CanvasResourceSharedImage::Bitmap() {
  TRACE_EVENT0("blink", "CanvasResourceSharedImage::Bitmap");

  // In order to avoid creating multiple representations for this shared image
  // on the same context, the AcceleratedStaticBitmapImage uses the texture id
  // of the resource here. We keep a count of pending shared image releases to
  // correctly scope the read lock for this texture.
  // If this resource is accessed across threads, or the
  // AcceleratedStaticBitmapImage is accessed on a different thread after being
  // created here, the image will create a new representation from the mailbox
  // rather than referring to the shared image's texture ID if it was provided
  // below.
  const bool has_read_ref_on_texture = !is_cross_thread();
  GLuint texture_id_for_image = 0u;
  if (has_read_ref_on_texture) {
    texture_id_for_image = owning_thread_data().texture_id;
    owning_thread_data().bitmap_image_read_refs++;
    if (owning_thread_data().bitmap_image_read_refs == 1u && ContextGL()) {
      ContextGL()->BeginSharedImageAccessDirectCHROMIUM(
          texture_id_for_image, GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
    }
  }

  // The |release_callback| keeps a ref on this resource to ensure the backing
  // shared image is kept alive until the lifetime of the image.
  // Note that the code in CanvasResourceProvider::RecycleResource also uses the
  // ref-count on the resource as a proxy for a read lock to allow recycling the
  // resource once all refs have been released.
  auto release_callback = viz::SingleReleaseCallback::Create(base::BindOnce(
      &OnBitmapImageDestroyed, scoped_refptr<CanvasResourceSharedImage>(this),
      has_read_ref_on_texture));

  scoped_refptr<StaticBitmapImage> image;
  SkImageInfo image_info = SkImageInfo::Make(
      Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
      ColorParams().GetSkAlphaType(), ColorParams().GetSkColorSpace());

  // If its cross thread, then the sync token was already verified. If not, then
  // it doesn't need to be verified.
  if (!is_cross_thread())
    owning_thread_data().mailbox_sync_mode = kUnverifiedSyncToken;

  image = AcceleratedStaticBitmapImage::CreateFromCanvasMailbox(
      mailbox(), is_cross_thread() ? sync_token() : GetSyncToken(),
      texture_id_for_image, image_info, texture_target_,
      context_provider_wrapper_, owning_thread_id_, is_origin_top_left_,
      std::move(release_callback));

  // The StaticBitmapImage is used for readbacks which may modify the texture
  // params. We reset this when the image is destroyed but it is important to
  // also do it here in case we try to send the resource to the display
  // compositor while the |image| is still alive.
  if (!is_cross_thread())
    owning_thread_data().needs_gl_filter_reset = true;

  DCHECK(image);
  return image;
}

const gpu::Mailbox& CanvasResourceSharedImage::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  if (!is_cross_thread()) {
    SetGLFilterIfNeeded();
    owning_thread_data().mailbox_sync_mode = sync_mode;
  }
  return mailbox();
}

void CanvasResourceSharedImage::SetGLFilterIfNeeded() {
  DCHECK(!is_cross_thread());

  if (!owning_thread_data().needs_gl_filter_reset || !ContextGL() ||
      !WeakProvider())
    return;

  ContextGL()->BindTexture(TextureTarget(), GetTextureIdForBackendTexture());
  ContextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GLFilter());
  ContextGL()->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLFilter());
  ContextGL()->BindTexture(TextureTarget(), 0u);
  owning_thread_data().mailbox_needs_new_sync_token = true;
  owning_thread_data().needs_gl_filter_reset = false;
  Provider()->NotifyTexParamsModified(this);
}

bool CanvasResourceSharedImage::HasGpuMailbox() const {
  return !mailbox().IsZero();
}

const gpu::SyncToken CanvasResourceSharedImage::GetSyncToken() {
  if (is_cross_thread()) {
    // Sync token should be generated at Transfer time, which must always be
    // called before cross-thread usage. And since we don't allow writes on
    // another thread, the sync token generated at Transfer time shouldn't
    // have been invalidated.
    DCHECK(!mailbox_needs_new_sync_token());
    DCHECK(sync_token().verified_flush());

    return sync_token();
  }

  if (mailbox_needs_new_sync_token()) {
    auto* gl = ContextGL();
    DCHECK(gl);  // caller should already have early exited if !gl.
    gl->GenUnverifiedSyncTokenCHROMIUM(
        owning_thread_data().sync_token.GetData());
    owning_thread_data().mailbox_needs_new_sync_token = false;
  }

  if (owning_thread_data().mailbox_sync_mode == kVerifiedSyncToken &&
      !owning_thread_data().sync_token.verified_flush()) {
    int8_t* token_data = owning_thread_data().sync_token.GetData();
    auto* gl = ContextGL();
    gl->ShallowFlushCHROMIUM();
    gl->VerifySyncTokensCHROMIUM(&token_data, 1);
    owning_thread_data().sync_token.SetVerifyFlush();
  }

  return sync_token();
}

void CanvasResourceSharedImage::NotifyResourceLost() {
  owning_thread_data().is_lost = true;

  // Since the texture params are in an unknown state, reset the cached tex
  // params state for the resource.
  owning_thread_data().needs_gl_filter_reset = true;
  if (WeakProvider())
    Provider()->NotifyTexParamsModified(this);
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
CanvasResourceSharedImage::ContextProviderWrapper() const {
  DCHECK(!is_cross_thread());
  return context_provider_wrapper_;
}

// ExternalCanvasResource
//==============================================================================
scoped_refptr<ExternalCanvasResource> ExternalCanvasResource::Create(
    const gpu::Mailbox& mailbox,
    const IntSize& size,
    GLenum texture_target,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality) {
  TRACE_EVENT0("blink", "ExternalCanvasResource::Create");
  auto resource = AdoptRef(
      new ExternalCanvasResource(mailbox, size, texture_target, color_params,
                                 std::move(context_provider_wrapper),
                                 std::move(provider), filter_quality));
  return resource->IsValid() ? resource : nullptr;
}

ExternalCanvasResource::~ExternalCanvasResource() {
  OnDestroy();
}

bool ExternalCanvasResource::IsValid() const {
  return !!context_provider_wrapper_ && !mailbox_.IsZero();
}

void ExternalCanvasResource::Abandon() {
  mailbox_.SetZero();
  sync_token_.Clear();
}

void ExternalCanvasResource::TakeSkImage(sk_sp<SkImage> image) {
  NOTREACHED();
}

scoped_refptr<StaticBitmapImage> ExternalCanvasResource::Bitmap() {
  NOTREACHED();
  return nullptr;
}

void ExternalCanvasResource::TearDown() {
  Abandon();
}

const gpu::Mailbox& ExternalCanvasResource::GetOrCreateGpuMailbox(
    MailboxSyncMode sync_mode) {
  TRACE_EVENT0("blink", "ExternalCanvasResource::GetOrCreateGpuMailbox");
  DCHECK_EQ(sync_mode, kVerifiedSyncToken);
  return mailbox_;
}

bool ExternalCanvasResource::HasGpuMailbox() const {
  return !mailbox_.IsZero();
}

const gpu::SyncToken ExternalCanvasResource::GetSyncToken() {
  TRACE_EVENT0("blink", "ExternalCanvasResource::GetSyncToken");
  if (!sync_token_.HasData()) {
    auto* gl = ContextGL();
    if (gl)
      gl->GenSyncTokenCHROMIUM(sync_token_.GetData());
  }
  return sync_token_;
}

base::WeakPtr<WebGraphicsContext3DProviderWrapper>
ExternalCanvasResource::ContextProviderWrapper() const {
  return context_provider_wrapper_;
}

ExternalCanvasResource::ExternalCanvasResource(
    const gpu::Mailbox& mailbox,
    const IntSize& size,
    GLenum texture_target,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceProvider> provider,
    SkFilterQuality filter_quality)
    : CanvasResource(std::move(provider), filter_quality, color_params),
      context_provider_wrapper_(std::move(context_provider_wrapper)),
      size_(size),
      texture_target_(texture_target),
      mailbox_(mailbox) {}

}  // namespace blink
