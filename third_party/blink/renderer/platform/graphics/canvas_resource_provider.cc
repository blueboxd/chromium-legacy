// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/stl_util.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/tiles/software_image_decode_cache.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/capabilities.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_info.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/graphics/canvas_heuristic_parameters.h"
#include "third_party/blink/renderer/platform/graphics/gpu/shared_gpu_context.h"
#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

void CanvasResourceProvider::RecordTypeToUMA(ResourceProviderType type) {
  base::UmaHistogramEnumeration("Blink.Canvas.ResourceProviderType", type);
}

// * Renders to a texture managed by Skia. Mailboxes are backed by vanilla GL
//   textures.
// * Layers are not overlay candidates.
class CanvasResourceProviderTexture : public CanvasResourceProvider {
 public:
  CanvasResourceProviderTexture(
      const IntSize& size,
      unsigned msaa_sample_count,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      bool is_origin_top_left)
      : CanvasResourceProvider(size,
                               color_params,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)),
        msaa_sample_count_(msaa_sample_count),
        is_origin_top_left_(is_origin_top_left) {
    RecordTypeToUMA(kTexture);
  }

  ~CanvasResourceProviderTexture() override = default;

  bool IsValid() const final { return GetSkSurface() && !IsGpuContextLost(); }
  bool IsAccelerated() const final { return true; }
  bool SupportsDirectCompositing() const override { return true; }

  GLuint GetBackingTextureHandleForOverwrite() override {
    GrBackendTexture backend_texture = GetSkSurface()->getBackendTexture(
        SkSurface::kDiscardWrite_TextureHandleAccess);
    if (!backend_texture.isValid())
      return 0;
    GrGLTextureInfo info;
    if (!backend_texture.getGLTextureInfo(&info))
      return 0;
    return info.fID;
  }

 protected:
  scoped_refptr<CanvasResource> ProduceCanvasResource() override {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderTexture::ProduceCanvasResource");
    DCHECK(GetSkSurface());

    if (IsGpuContextLost())
      return nullptr;

    auto* gl = ContextGL();
    DCHECK(gl);

    if (ContextProviderWrapper()
            ->ContextProvider()
            ->GetCapabilities()
            .disable_2d_canvas_copy_on_write) {
      // A readback operation may alter the texture parameters, which may affect
      // the compositor's behavior. Therefore, we must trigger copy-on-write
      // even though we are not technically writing to the texture, only to its
      // parameters. This issue is Android-WebView specific: crbug.com/585250.
      // If this issue with readback affecting state is ever fixed, then we'll
      // have to do this instead of triggering a copy-on-write:
      // static_cast<AcceleratedStaticBitmapImage*>(image.get())
      //  ->RetainOriginalSkImageForCopyOnWrite();
      GetSkSurface()->notifyContentWillChange(
          SkSurface::kRetain_ContentChangeMode);
    }

    auto paint_image = MakeImageSnapshot();
    if (!paint_image)
      return nullptr;
    DCHECK(paint_image.GetSkImage()->isTextureBacked());

    scoped_refptr<StaticBitmapImage> image = StaticBitmapImage::Create(
        paint_image.GetSkImage(), ContextProviderWrapper());

    scoped_refptr<CanvasResource> resource = CanvasResourceBitmap::Create(
        image, CreateWeakPtr(), FilterQuality(), ColorParams());
    if (!resource)
      return nullptr;

    return resource;
  }

  scoped_refptr<StaticBitmapImage> Snapshot() override {
    TRACE_EVENT0("blink", "CanvasResourceProviderTexture::Snapshot");
    return SnapshotInternal();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderTexture::CreateSkSurface");
    if (IsGpuContextLost())
      return nullptr;
    auto* gr = GetGrContext();
    DCHECK(gr);

    const SkImageInfo info = SkImageInfo::Make(
        Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
        kPremul_SkAlphaType, ColorParams().GetSkColorSpaceForSkSurfaces());

    const enum GrSurfaceOrigin surface_origin =
        is_origin_top_left_ ? kTopLeft_GrSurfaceOrigin
                            : kBottomLeft_GrSurfaceOrigin;

    return SkSurface::MakeRenderTarget(gr, SkBudgeted::kNo, info,
                                       msaa_sample_count_, surface_origin,
                                       ColorParams().GetSkSurfaceProps());
  }

  const unsigned msaa_sample_count_;
  const bool is_origin_top_left_;
};

// * Renders to a texture managed by Skia. Mailboxes are GPU-accelerated
//   platform native surfaces.
// * Layers are overlay candidates.
class CanvasResourceProviderTextureGpuMemoryBuffer final
    : public CanvasResourceProviderTexture {
 public:
  CanvasResourceProviderTextureGpuMemoryBuffer(
      const IntSize& size,
      unsigned msaa_sample_count,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      bool is_origin_top_left)
      : CanvasResourceProviderTexture(size,
                                      msaa_sample_count,
                                      color_params,
                                      std::move(context_provider_wrapper),
                                      std::move(resource_dispatcher),
                                      is_origin_top_left) {
    RecordTypeToUMA(kTextureGpuMemoryBuffer);
  }

  ~CanvasResourceProviderTextureGpuMemoryBuffer() override = default;
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0(
        "blink",
        "CanvasResourceProviderTextureGpuMemoreBuffer::CreateResource");
    constexpr bool is_accelerated = true;
    return CanvasResourceGpuMemoryBuffer::Create(
        Size(), ColorParams(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality(), is_accelerated);
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource() final {
    TRACE_EVENT0(
        "blink",
        "CanvasResourceProviderTextureGpuMemoreBuffer::ProduceCanvasResource");
    DCHECK(GetSkSurface());

    if (IsGpuContextLost())
      return nullptr;

    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource) {
      // GpuMemoryBuffer creation failed, fallback to Texture resource
      return CanvasResourceProviderTexture::ProduceCanvasResource();
    }

    auto paint_image = MakeImageSnapshot();
    if (!paint_image)
      return nullptr;
    DCHECK(paint_image.GetSkImage()->isTextureBacked());

    GrBackendTexture backend_texture =
        paint_image.GetSkImage()->getBackendTexture(true);
    DCHECK(backend_texture.isValid());

    GrGLTextureInfo info;
    if (!backend_texture.getGLTextureInfo(&info))
      return nullptr;

    GLuint skia_texture_id = info.fID;
    output_resource->CopyFromTexture(skia_texture_id,
                                     ColorParams().GLUnsizedInternalFormat(),
                                     ColorParams().GLType());
    return output_resource;
  }
};

// * Renders to a Skia RAM-backed bitmap.
// * Mailboxing is not supported : cannot be directly composited.
class CanvasResourceProviderBitmap : public CanvasResourceProvider {
 public:
  CanvasResourceProviderBitmap(
      const IntSize& size,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProvider(size,
                               color_params,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)) {
    RecordTypeToUMA(kBitmap);
  }

  ~CanvasResourceProviderBitmap() override = default;

  bool IsValid() const final { return GetSkSurface(); }
  bool IsAccelerated() const final { return false; }
  bool SupportsDirectCompositing() const override { return false; }

 private:
  scoped_refptr<CanvasResource> ProduceCanvasResource() override {
    return nullptr;  // Does not support direct compositing
  }

  scoped_refptr<StaticBitmapImage> Snapshot() override {
    TRACE_EVENT0("blink", "CanvasResourceProviderBitmap::Snapshot");
    return SnapshotInternal();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderBitmap::CreateSkSurface");

    SkImageInfo info = SkImageInfo::Make(
        Size().Width(), Size().Height(), ColorParams().GetSkColorType(),
        kPremul_SkAlphaType, ColorParams().GetSkColorSpaceForSkSurfaces());
    return SkSurface::MakeRaster(info, ColorParams().GetSkSurfaceProps());
  }
};

// * Renders to a ram memory buffer managed by Skia
// * Uses GpuMemoryBuffer to pass frames to the compositor
// * Layers are overlay candidates
class CanvasResourceProviderBitmapGpuMemoryBuffer final
    : public CanvasResourceProviderBitmap {
 public:
  CanvasResourceProviderBitmapGpuMemoryBuffer(
      const IntSize& size,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProviderBitmap(size,
                                     color_params,
                                     std::move(context_provider_wrapper),
                                     std::move(resource_dispatcher)) {
    RecordTypeToUMA(kBitmapGpuMemoryBuffer);
  }

  ~CanvasResourceProviderBitmapGpuMemoryBuffer() override = default;
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderBitmapGpuMemoryBuffer::CreateResource");

    constexpr bool is_accelerated = false;
    return CanvasResourceGpuMemoryBuffer::Create(
        Size(), ColorParams(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality(), is_accelerated);
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource() final {
    TRACE_EVENT0(
        "blink",
        "CanvasResourceProviderBitmapGpuMemoryBuffer::ProduceCanvasResource");

    DCHECK(GetSkSurface());

    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource) {
      // Not compositable without a GpuMemoryBuffer
      return nullptr;
    }

    auto paint_image = MakeImageSnapshot();
    if (!paint_image)
      return nullptr;
    DCHECK(!paint_image.GetSkImage()->isTextureBacked());

    output_resource->TakeSkImage(paint_image.GetSkImage());

    return output_resource;
  }
};

// * Renders to a shared memory bitmap.
// * Uses SharedBitmaps to pass frames directly to the compositor.
class CanvasResourceProviderSharedBitmap : public CanvasResourceProviderBitmap {
 public:
  CanvasResourceProviderSharedBitmap(
      const IntSize& size,
      const CanvasColorParams color_params,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProviderBitmap(size,
                                     color_params,
                                     nullptr,  // context_provider_wrapper
                                     std::move(resource_dispatcher)) {
    DCHECK(ResourceDispatcher());
    RecordTypeToUMA(kSharedBitmap);
  }
  ~CanvasResourceProviderSharedBitmap() override = default;
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    CanvasColorParams color_params = ColorParams();
    if (!IsBitmapFormatSupported(color_params.TransferableResourceFormat())) {
      // If the rendering format is not supported, downgrate to 8-bits.
      // TODO(junov): Should we try 12-12-12-12 and 10-10-10-2?
      color_params.SetCanvasPixelFormat(kRGBA8CanvasPixelFormat);
    }

    return CanvasResourceSharedBitmap::Create(Size(), color_params,
                                              CreateWeakPtr(), FilterQuality());
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource() final {
    DCHECK(GetSkSurface());
    scoped_refptr<CanvasResource> output_resource = NewOrRecycledResource();
    if (!output_resource)
      return nullptr;

    auto paint_image = MakeImageSnapshot();
    if (!paint_image)
      return nullptr;
    DCHECK(!paint_image.GetSkImage()->isTextureBacked());

    output_resource->TakeSkImage(paint_image.GetSkImage());

    return output_resource;
  }
};

// * Renders to a GpuMemoryBuffer-backed texture used to create a SkSurface.
// * Layers are overlay candidates
class CanvasResourceProviderDirectGpuMemoryBuffer final
    : public CanvasResourceProvider {
 public:
  CanvasResourceProviderDirectGpuMemoryBuffer(
      const IntSize& size,
      unsigned msaa_sample_count,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      bool is_origin_top_left)
      : CanvasResourceProvider(size,
                               color_params,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)),
        msaa_sample_count_(msaa_sample_count),
        is_origin_top_left_(is_origin_top_left) {
    constexpr bool is_accelerated = true;
    resource_ = CanvasResourceGpuMemoryBuffer::Create(
        Size(), ColorParams(), ContextProviderWrapper(), CreateWeakPtr(),
        FilterQuality(), is_accelerated);
  }

  ~CanvasResourceProviderDirectGpuMemoryBuffer() override = default;
  bool IsValid() const final { return GetSkSurface() && !IsGpuContextLost(); }
  bool IsAccelerated() const final { return true; }
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  GLuint GetBackingTextureHandleForOverwrite() override {
    return resource_->GetBackingTextureHandleForOverwrite();
  }

  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderDirectGpuMemoryBuffer::CreateResource");
    DCHECK(resource_);
    return resource_;
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource() final {
    TRACE_EVENT0(
        "blink",
        "CanvasResourceProviderDirectGpuMemoryBuffer::ProduceCanvasResource");
    if (IsGpuContextLost())
      return nullptr;
    FlushSkia();

    auto* gl = ContextGL();
    DCHECK(gl);
    gl->Flush();

    return NewOrRecycledResource();
  }

  scoped_refptr<StaticBitmapImage> Snapshot() override {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderDirectGpuMemoryBuffer::Snapshot");
    return SnapshotInternal();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    if (IsGpuContextLost() || !resource_)
      return nullptr;
    auto* gr = GetGrContext();
    DCHECK(gr);

    GrGLTextureInfo texture_info = {};
    texture_info.fID = resource_->GetBackingTextureHandleForOverwrite();
    texture_info.fTarget = GL_TEXTURE_2D;
    // Skia requires a sized internal format.
    texture_info.fFormat = ColorParams().GLSizedInternalFormat();

    const GrBackendTexture backend_texture(Size().Width(), Size().Height(),
                                           GrMipMapped::kNo, texture_info);

    const enum GrSurfaceOrigin surface_origin =
        is_origin_top_left_ ? kTopLeft_GrSurfaceOrigin
                            : kBottomLeft_GrSurfaceOrigin;

    auto surface = SkSurface::MakeFromBackendTextureAsRenderTarget(
        gr, backend_texture, surface_origin, msaa_sample_count_,
        ColorParams().GetSkColorType(),
        ColorParams().GetSkColorSpaceForSkSurfaces(),
        ColorParams().GetSkSurfaceProps());
    return surface;
  }

  const unsigned msaa_sample_count_;
  const bool is_origin_top_left_;
  scoped_refptr<CanvasResource> resource_;
};

// * Renders to a SharedImage, which manages memory internally.
// * Layers are overlay candidates.
class CanvasResourceProviderSharedImage : public CanvasResourceProvider {
 public:
  CanvasResourceProviderSharedImage(
      const IntSize& size,
      unsigned msaa_sample_count,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
      bool is_origin_top_left,
      bool is_overlay_candidate)
      : CanvasResourceProvider(size,
                               color_params,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)),
        msaa_sample_count_(msaa_sample_count),
        is_origin_top_left_(is_origin_top_left),
        is_overlay_candidate_(is_overlay_candidate) {
    RecordTypeToUMA(kSharedImage);
    resource_ = NewOrRecycledResource();
    if (resource_)
      EnsureWriteAccess();
  }

  ~CanvasResourceProviderSharedImage() override {}

  bool IsValid() const final { return GetSkSurface() && !IsGpuContextLost(); }
  bool IsAccelerated() const final { return true; }
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return false; }
  GLuint GetBackingTextureHandleForOverwrite() override {
    if (!ContextProviderWrapper())
      return 0u;

    FlushGrContext();
    WillDraw();
    return resource()->GetTextureIdForBackendTexture();
  }

  scoped_refptr<CanvasResource> CreateResource() final {
    TRACE_EVENT0("blink", "CanvasResourceProviderSharedImage::CreateResource");
    return CanvasResourceSharedImage::Create(
        Size(), ContextProviderWrapper(), CreateWeakPtr(), FilterQuality(),
        ColorParams(), is_overlay_candidate_, is_origin_top_left_);
  }

  void NotifyTexParamsModified(const CanvasResource* resource) override {
    if (resource_.get() == resource) {
      DCHECK(!current_resource_has_write_access_);
      // Note that the call below is guarenteed to not issue any GPU work for
      // the backend texture since we ensure that all skia work on the resource
      // is issued before releasing write access.
      surface_->getBackendTexture(SkSurface::kFlushRead_BackendHandleAccess)
          .glTextureParametersModified();
    }
  }

 protected:
  scoped_refptr<CanvasResource> ProduceCanvasResource() override {
    TRACE_EVENT0("blink",
                 "CanvasResourceProviderSharedImage::ProduceCanvasResource");
    // Its important to end read access and ref the resource before the WillDraw
    // call below. Since it relies on resource ref-count to trigger
    // copy-on-write and asserts that we only have write access when the
    // provider has the only ref to the resource, to ensure there are no other
    // readers.
    EndWriteAccess();
    scoped_refptr<CanvasResource> resource = resource_;
    if (ContextProviderWrapper()
            ->ContextProvider()
            ->GetCapabilities()
            .disable_2d_canvas_copy_on_write) {
      // A readback operation may alter the texture parameters, which may affect
      // the compositor's behavior. Therefore, we must trigger copy-on-write
      // even though we are not technically writing to the texture, only to its
      // parameters. This issue is Android-WebView specific: crbug.com/585250.
      WillDraw();
    }

    return resource;
  }

  scoped_refptr<StaticBitmapImage> Snapshot() override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSharedImage::Snapshot");
    EndWriteAccess();
    return resource_->Bitmap();
  }

  void WillDraw() override {
    DCHECK(resource_);

    if (!ContextProviderWrapper())
      return;

    // If |resource_| is still being used by the compositor we need to create
    // a new resource or reuse a previously recycled one. This class holds one
    // reference so we check if there is more than one.
    if (!resource_->HasOneRef()) {
      DCHECK(!current_resource_has_write_access_)
          << "Write access must be released before sharing the resource";

      auto* old_resource =
          static_cast<CanvasResourceSharedImage*>(resource_.get());
      resource_ = NewOrRecycledResource();
      EnsureWriteAccess();
      if (surface_) {
        // Take read access to the outgoing resource for the skia copy below.
        ContextGL()->BeginSharedImageAccessDirectCHROMIUM(
            old_resource->GetTextureIdForBackendTexture(),
            GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM);
        surface_->replaceBackendTexture(CreateGrTextureForResource(),
                                        GetGrSurfaceOrigin());
        surface_->flush();
        ContextGL()->EndSharedImageAccessDirectCHROMIUM(
            old_resource->GetTextureIdForBackendTexture());
      }
    }

    EnsureWriteAccess();
    resource()->WillDraw();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    TRACE_EVENT0("blink", "CanvasResourceProviderSharedImage::CreateSkSurface");
    if (IsGpuContextLost())
      return nullptr;

    return SkSurface::MakeFromBackendTexture(
        GetGrContext(), CreateGrTextureForResource(), GetGrSurfaceOrigin(),
        msaa_sample_count_, ColorParams().GetSkColorType(),
        ColorParams().GetSkColorSpaceForSkSurfaces(),
        ColorParams().GetSkSurfaceProps());
  }

  GrSurfaceOrigin GetGrSurfaceOrigin() const {
    return is_origin_top_left_ ? kTopLeft_GrSurfaceOrigin
                               : kBottomLeft_GrSurfaceOrigin;
  }

  GrBackendTexture CreateGrTextureForResource() const {
    GrGLTextureInfo texture_info = {};
    texture_info.fID = resource()->GetTextureIdForBackendTexture();
    texture_info.fTarget = resource_->TextureTarget();
    texture_info.fFormat = ColorParams().GLSizedInternalFormat();
    return GrBackendTexture(Size().Width(), Size().Height(), GrMipMapped::kNo,
                            texture_info);
  }

  void FlushGrContext() {
    // The resource may have been imported and used in skia. Make sure any
    // operations using this resource are flushed to the underlying context.
    // Note that its not sufficient to flush the SkSurface here since it will
    // only perform a GrContext flush if that SkSurface has any pending ops. And
    // this resource may be written to or read from skia without using the
    // SkSurface here.
    GetGrContext()->flush();
  }

  void EnsureWriteAccess() {
    DCHECK(resource_);
    DCHECK(resource_->HasOneRef())
        << "Write access requires exclusive access to the resource";
    DCHECK(!resource()->is_cross_thread())
        << "Write access is only allowed on the owning thread";

    if (current_resource_has_write_access_ || !ContextProviderWrapper())
      return;

    auto texture_id = resource()->GetTextureIdForBackendTexture();
    ContextGL()->BeginSharedImageAccessDirectCHROMIUM(
        texture_id, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
    current_resource_has_write_access_ = true;
  }

  void EndWriteAccess() {
    DCHECK(!resource()->is_cross_thread());

    if (!current_resource_has_write_access_ || !ContextProviderWrapper())
      return;

    // Issue any skia work using this resource before releasing write access.
    FlushGrContext();
    auto texture_id = resource()->GetTextureIdForBackendTexture();
    ContextGL()->EndSharedImageAccessDirectCHROMIUM(texture_id);
    current_resource_has_write_access_ = false;
  }

  CanvasResourceSharedImage* resource() {
    return static_cast<CanvasResourceSharedImage*>(resource_.get());
  }
  const CanvasResourceSharedImage* resource() const {
    return static_cast<const CanvasResourceSharedImage*>(resource_.get());
  }

  const unsigned msaa_sample_count_;
  const bool is_origin_top_left_;
  const bool is_overlay_candidate_;
  bool current_resource_has_write_access_ = false;
  scoped_refptr<CanvasResource> resource_;
};

// This class does nothing except answering to ProduceCanvasResource() by piping
// it to NewOrRecycledResource().  This ResourceProvider is meant to be used
// with an imported external CanvasResource, and all drawing and lifetime logic
// must be kept at a higher level.
class CanvasResourceProviderPassThrough final : public CanvasResourceProvider {
 public:
  CanvasResourceProviderPassThrough(
      const IntSize& size,
      const CanvasColorParams color_params,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>
          context_provider_wrapper,
      base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
      : CanvasResourceProvider(size,
                               color_params,
                               std::move(context_provider_wrapper),
                               std::move(resource_dispatcher)) {}

  ~CanvasResourceProviderPassThrough() override = default;
  bool IsValid() const final { return true; }
  bool IsAccelerated() const final { return true; }
  bool SupportsDirectCompositing() const override { return true; }
  bool SupportsSingleBuffering() const override { return true; }

 private:
  scoped_refptr<CanvasResource> CreateResource() final {
    // This class has no CanvasResource to provide: this must be imported via
    // ImportResource() and kept in the parent class.
    NOTREACHED();
    return nullptr;
  }

  scoped_refptr<CanvasResource> ProduceCanvasResource() final {
    return NewOrRecycledResource();
  }

  sk_sp<SkSurface> CreateSkSurface() const override {
    NOTREACHED();
    return nullptr;
  }

  scoped_refptr<StaticBitmapImage> Snapshot() override {
    NOTREACHED();
    return nullptr;
  }
};

namespace {

enum class CanvasResourceType {
  kDirect3DGpuMemoryBuffer,
  kDirect2DGpuMemoryBuffer,
  kTextureGpuMemoryBuffer,
  kBitmapGpuMemoryBuffer,
  kDirect3DSwapChain,
  kSharedBitmap,
  kTexture,
  kBitmap,
  kSharedImage,
};

const std::vector<CanvasResourceType>& GetResourceTypeFallbackList(
    CanvasResourceProvider::ResourceUsage usage) {
  static const std::vector<CanvasResourceType> kSoftwareFallbackList({
      CanvasResourceType::kBitmap,
  });

  static const std::vector<CanvasResourceType> kAcceleratedFallbackList({
      CanvasResourceType::kSharedImage,
      CanvasResourceType::kTexture,
      // Fallback to software
      CanvasResourceType::kBitmap,
  });

  static const std::vector<CanvasResourceType> kSoftwareCompositedFallbackList({
      CanvasResourceType::kBitmapGpuMemoryBuffer,
      CanvasResourceType::kSharedBitmap,
      // Fallback to no direct compositing support
      CanvasResourceType::kBitmap,
  });

  static const std::vector<CanvasResourceType>
      kAcceleratedCompositedFallbackList({
          CanvasResourceType::kSharedImage,
          CanvasResourceType::kTextureGpuMemoryBuffer,
          CanvasResourceType::kTexture,
          // Fallback to software composited
          // (|kSoftwareCompositedFallbackList|).
          CanvasResourceType::kBitmapGpuMemoryBuffer,
          CanvasResourceType::kSharedBitmap,
          // Fallback to no direct compositing support
          CanvasResourceType::kBitmap,
      });
  DCHECK(std::equal(kAcceleratedCompositedFallbackList.begin() + 3,
                    kAcceleratedCompositedFallbackList.end(),
                    kSoftwareCompositedFallbackList.begin(),
                    kSoftwareCompositedFallbackList.end()));

  static const std::vector<CanvasResourceType> kAcceleratedDirect2DFallbackList(
      {
          // TODO(khushalsagar): This is used for low-latency canvas. We'll need
          // support for single buffering to use shared images here.
          CanvasResourceType::kDirect2DGpuMemoryBuffer,
          // The rest is equal to |kAcceleratedCompositedFallbackList|.
          CanvasResourceType::kSharedImage,
          CanvasResourceType::kTextureGpuMemoryBuffer,
          CanvasResourceType::kTexture,
          // Fallback to software composited
          CanvasResourceType::kBitmapGpuMemoryBuffer,
          CanvasResourceType::kSharedBitmap,
          // Fallback to no direct compositing support
          CanvasResourceType::kBitmap,
      });
  DCHECK(std::equal(kAcceleratedDirect2DFallbackList.begin() + 1,
                    kAcceleratedDirect2DFallbackList.end(),
                    kAcceleratedCompositedFallbackList.begin(),
                    kAcceleratedCompositedFallbackList.end()));

  static const std::vector<CanvasResourceType> kAcceleratedDirect3DFallbackList(
      {
          // This is used with single-buffered WebGL where the resource comes
          // from an external source. The external site should take care of
          // using SharedImages since the resource will be used by the display
          // compositor.
          CanvasResourceType::kDirect3DSwapChain,
          CanvasResourceType::kDirect3DGpuMemoryBuffer,
          CanvasResourceType::kDirect2DGpuMemoryBuffer,
          // The rest is equal to |kAcceleratedCompositedFallbackList|.
          CanvasResourceType::kSharedImage,
          CanvasResourceType::kTextureGpuMemoryBuffer,
          CanvasResourceType::kTexture,
          // Fallback to software composited
          CanvasResourceType::kBitmapGpuMemoryBuffer,
          CanvasResourceType::kSharedBitmap,
          // Fallback to no direct compositing support
          CanvasResourceType::kBitmap,
      });
  DCHECK(std::equal(kAcceleratedDirect3DFallbackList.begin() + 2,
                    kAcceleratedDirect3DFallbackList.end(),
                    kAcceleratedDirect2DFallbackList.begin(),
                    kAcceleratedDirect2DFallbackList.end()));

  switch (usage) {
    case CanvasResourceProvider::kSoftwareResourceUsage:
      return kSoftwareFallbackList;
    case CanvasResourceProvider::kSoftwareCompositedResourceUsage:
      return kSoftwareCompositedFallbackList;
    case CanvasResourceProvider::kAcceleratedResourceUsage:
      return kAcceleratedFallbackList;
    case CanvasResourceProvider::kAcceleratedCompositedResourceUsage:
      return kAcceleratedCompositedFallbackList;
    case CanvasResourceProvider::kAcceleratedDirect2DResourceUsage:
      return kAcceleratedDirect2DFallbackList;
    case CanvasResourceProvider::kAcceleratedDirect3DResourceUsage:
      return kAcceleratedDirect3DFallbackList;
  }
  NOTREACHED();
}
}  // unnamed namespace

std::unique_ptr<CanvasResourceProvider> CanvasResourceProvider::Create(
    const IntSize& size,
    ResourceUsage usage,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    unsigned msaa_sample_count,
    const CanvasColorParams& color_params,
    PresentationMode presentation_mode,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
    bool is_origin_top_left) {
  std::unique_ptr<CanvasResourceProvider> provider;
  const std::vector<CanvasResourceType>& fallback_list =
      GetResourceTypeFallbackList(usage);

  const bool is_gpu_memory_buffer_image_allowed =
      SharedGpuContext::IsGpuCompositingEnabled() && context_provider_wrapper &&
      presentation_mode == kAllowImageChromiumPresentationMode &&
      gpu::IsImageSizeValidForGpuMemoryBufferFormat(
          gfx::Size(size), color_params.GetBufferFormat()) &&
      gpu::IsImageFromGpuMemoryBufferFormatSupported(
          color_params.GetBufferFormat(),
          context_provider_wrapper->ContextProvider()->GetCapabilities());

  // TODO(ashithasantosh): Include checks for capabilities, format and size.
  const bool is_swap_chain_allowed =
      presentation_mode == kAllowSwapChainPresentationMode;

  for (CanvasResourceType resource_type : fallback_list) {
    // Note: We are deliberately not using std::move() on
    // |context_provider_wrapper| and |resource_dispatcher| to ensure that the
    // pointers remain valid for the next iteration of this loop if necessary.
    switch (resource_type) {
      case CanvasResourceType::kDirect3DSwapChain:
        if (!is_swap_chain_allowed)
          continue;
        provider = std::make_unique<CanvasResourceProviderPassThrough>(
            size, color_params, context_provider_wrapper, resource_dispatcher);
        break;
      case CanvasResourceType::kTextureGpuMemoryBuffer:
        if (!is_gpu_memory_buffer_image_allowed)
          continue;
        DCHECK_EQ(color_params.GLUnsizedInternalFormat(),
                  gpu::InternalFormatForGpuMemoryBufferFormat(
                      color_params.GetBufferFormat()));
        provider =
            std::make_unique<CanvasResourceProviderTextureGpuMemoryBuffer>(
                size, msaa_sample_count, color_params, context_provider_wrapper,
                resource_dispatcher, is_origin_top_left);
        break;
      case CanvasResourceType::kDirect2DGpuMemoryBuffer:
        if (!is_gpu_memory_buffer_image_allowed)
          continue;
        DCHECK_EQ(color_params.GLUnsizedInternalFormat(),
                  gpu::InternalFormatForGpuMemoryBufferFormat(
                      color_params.GetBufferFormat()));
        provider =
            std::make_unique<CanvasResourceProviderDirectGpuMemoryBuffer>(
                size, msaa_sample_count, color_params, context_provider_wrapper,
                resource_dispatcher, is_origin_top_left);
        break;
      case CanvasResourceType::kDirect3DGpuMemoryBuffer:
        if (!is_gpu_memory_buffer_image_allowed)
          continue;
        DCHECK_EQ(color_params.GLUnsizedInternalFormat(),
                  gpu::InternalFormatForGpuMemoryBufferFormat(
                      color_params.GetBufferFormat()));
        provider = std::make_unique<CanvasResourceProviderPassThrough>(
            size, color_params, context_provider_wrapper, resource_dispatcher);
        break;
      case CanvasResourceType::kBitmapGpuMemoryBuffer:
        if (!is_gpu_memory_buffer_image_allowed ||
            !Platform::Current()->GetGpuMemoryBufferManager()) {
          continue;
        }
        provider =
            std::make_unique<CanvasResourceProviderBitmapGpuMemoryBuffer>(
                size, color_params, context_provider_wrapper,
                resource_dispatcher);
        break;
      case CanvasResourceType::kSharedBitmap:
        if (!resource_dispatcher)
          continue;
        provider = std::make_unique<CanvasResourceProviderSharedBitmap>(
            size, color_params, resource_dispatcher);
        break;
      case CanvasResourceType::kTexture:
        if (!context_provider_wrapper)
          continue;
        provider = std::make_unique<CanvasResourceProviderTexture>(
            size, msaa_sample_count, color_params, context_provider_wrapper,
            resource_dispatcher, is_origin_top_left);
        break;
      case CanvasResourceType::kBitmap:
        provider = std::make_unique<CanvasResourceProviderBitmap>(
            size, color_params, context_provider_wrapper, resource_dispatcher);
        break;
      case CanvasResourceType::kSharedImage: {
        // TODO(khushalsagar): Also kAcceleratedDirect2DResourceUsage when we
        // switch it to use shared images.
        const bool is_overlay_candidate =
            usage == kAcceleratedCompositedResourceUsage &&
            is_gpu_memory_buffer_image_allowed &&
            context_provider_wrapper->ContextProvider()
                ->GetCapabilities()
                .texture_storage_image;
        provider = std::make_unique<CanvasResourceProviderSharedImage>(
            size, msaa_sample_count, color_params, context_provider_wrapper,
            resource_dispatcher, is_origin_top_left, is_overlay_candidate);
      } break;
    }
    if (!provider->IsValid())
      continue;
    base::UmaHistogramBoolean("Blink.Canvas.ResourceProviderIsAccelerated",
                              provider->IsAccelerated());
    return provider;
  }

  return nullptr;
}

std::unique_ptr<CanvasResourceProvider>
CanvasResourceProvider::CreateForTesting(
    const IntSize& size,
    ResourceProviderType type,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    unsigned msaa_sample_count,
    const CanvasColorParams& color_params,
    PresentationMode mode,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher,
    bool is_origin_top_left) {
  switch (type) {
    case CanvasResourceProvider::kSharedBitmap:
      DCHECK(resource_dispatcher);
      return std::make_unique<CanvasResourceProviderSharedBitmap>(
          size, color_params, resource_dispatcher);
    case CanvasResourceProvider::kTexture:
      DCHECK(context_provider_wrapper);
      return std::make_unique<CanvasResourceProviderTexture>(
          size, msaa_sample_count, color_params, context_provider_wrapper,
          resource_dispatcher, is_origin_top_left);
    case CanvasResourceProvider::kBitmap:
      return std::make_unique<CanvasResourceProviderBitmap>(
          size, color_params, context_provider_wrapper, resource_dispatcher);
    case CanvasResourceProvider::kSharedImage:
      return std::make_unique<CanvasResourceProviderSharedImage>(
          size, msaa_sample_count, color_params, context_provider_wrapper,
          resource_dispatcher, is_origin_top_left, false);
    case CanvasResourceProvider::kTextureGpuMemoryBuffer:
      return std::make_unique<CanvasResourceProviderTextureGpuMemoryBuffer>(
          size, msaa_sample_count, color_params, context_provider_wrapper,
          resource_dispatcher, is_origin_top_left);
    case CanvasResourceProvider::kBitmapGpuMemoryBuffer:
      return std::make_unique<CanvasResourceProviderBitmapGpuMemoryBuffer>(
          size, color_params, context_provider_wrapper, resource_dispatcher);
  }

  NOTREACHED();
  return nullptr;
}

class CanvasResourceProvider::CanvasImageProvider : public cc::ImageProvider {
 public:
  CanvasImageProvider(cc::ImageDecodeCache* cache_n32,
                      cc::ImageDecodeCache* cache_f16,
                      const gfx::ColorSpace& target_color_space,
                      SkColorType target_color_type,
                      bool is_hardware_decode_cache);
  ~CanvasImageProvider() override = default;

  // cc::ImageProvider implementation.
  cc::ImageProvider::ScopedResult GetRasterContent(
      const cc::DrawImage&) override;

  void ReleaseLockedImages() { locked_images_.clear(); }

 private:
  void CanUnlockImage(ScopedResult);
  void CleanupLockedImages();

  bool is_hardware_decode_cache_;
  bool cleanup_task_pending_ = false;
  std::vector<ScopedResult> locked_images_;
  cc::PlaybackImageProvider playback_image_provider_n32_;
  base::Optional<cc::PlaybackImageProvider> playback_image_provider_f16_;

  base::WeakPtrFactory<CanvasImageProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CanvasImageProvider);
};

CanvasResourceProvider::CanvasImageProvider::CanvasImageProvider(
    cc::ImageDecodeCache* cache_n32,
    cc::ImageDecodeCache* cache_f16,
    const gfx::ColorSpace& target_color_space,
    SkColorType canvas_color_type,
    bool is_hardware_decode_cache)
    : is_hardware_decode_cache_(is_hardware_decode_cache),
      playback_image_provider_n32_(cache_n32,
                                   target_color_space,
                                   cc::PlaybackImageProvider::Settings()),
      weak_factory_(this) {
  // If the image provider may require to decode to half float instead of
  // uint8, create a f16 PlaybackImageProvider with the passed cache.
  if (canvas_color_type == kRGBA_F16_SkColorType) {
    DCHECK(cache_f16);
    playback_image_provider_f16_.emplace(cache_f16, target_color_space,
                                         cc::PlaybackImageProvider::Settings());
  }
}

cc::ImageProvider::ScopedResult
CanvasResourceProvider::CanvasImageProvider::GetRasterContent(
    const cc::DrawImage& draw_image) {
  // TODO(xidachen): Ensure this function works for paint worklet generated
  // images.
  // If we like to decode high bit depth image source to half float backed
  // image, we need to sniff the image bit depth here to avoid double decoding.
  ImageProvider::ScopedResult scoped_decoded_image;
  if (playback_image_provider_f16_ &&
      draw_image.paint_image().is_high_bit_depth()) {
    DCHECK(playback_image_provider_f16_);
    scoped_decoded_image =
        playback_image_provider_f16_->GetRasterContent(draw_image);
  } else {
    scoped_decoded_image =
        playback_image_provider_n32_.GetRasterContent(draw_image);
  }

  // Holding onto locked images here is a performance optimization for the
  // gpu image decode cache.  For that cache, it is expensive to lock and
  // unlock gpu discardable, and so it is worth it to hold the lock on
  // these images across multiple potential decodes.  In the software case,
  // locking in this manner makes it easy to run out of discardable memory
  // (backed by shared memory sometimes) because each per-colorspace image
  // decode cache has its own limit.  In the software case, just unlock
  // immediately and let the discardable system manage the cache logic
  // behind the scenes.
  if (!scoped_decoded_image.needs_unlock() || !is_hardware_decode_cache_) {
    return scoped_decoded_image;
  }

  constexpr int kMaxLockedImagesCount = 500;
  if (!scoped_decoded_image.decoded_image().is_budgeted() ||
      locked_images_.size() > kMaxLockedImagesCount) {
    // If we have exceeded the budget, ReleaseLockedImages any locked decodes.
    ReleaseLockedImages();
  }

  auto decoded_draw_image = scoped_decoded_image.decoded_image();
  return ScopedResult(decoded_draw_image,
                      base::BindOnce(&CanvasImageProvider::CanUnlockImage,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(scoped_decoded_image)));
}

void CanvasResourceProvider::CanvasImageProvider::CanUnlockImage(
    ScopedResult image) {
  // We should early out and avoid calling this function for software decodes.
  DCHECK(is_hardware_decode_cache_);

  // Because these image decodes are being done in javascript calling into
  // canvas code, there's no obvious time to do the cleanup.  To handle this,
  // post a cleanup task to run after javascript is done running.
  if (!cleanup_task_pending_) {
    cleanup_task_pending_ = true;
    Thread::Current()->GetTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&CanvasImageProvider::CleanupLockedImages,
                                  weak_factory_.GetWeakPtr()));
  }

  locked_images_.push_back(std::move(image));
}

void CanvasResourceProvider::CanvasImageProvider::CleanupLockedImages() {
  cleanup_task_pending_ = false;
  ReleaseLockedImages();
}

CanvasResourceProvider::CanvasResourceProvider(
    const IntSize& size,
    const CanvasColorParams& color_params,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper,
    base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher)
    : context_provider_wrapper_(std::move(context_provider_wrapper)),
      resource_dispatcher_(resource_dispatcher),
      size_(size),
      color_params_(color_params),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()),
      weak_ptr_factory_(this) {
  if (context_provider_wrapper_)
    context_provider_wrapper_->AddObserver(this);
}

CanvasResourceProvider::~CanvasResourceProvider() {
  if (context_provider_wrapper_)
    context_provider_wrapper_->RemoveObserver(this);
}

SkSurface* CanvasResourceProvider::GetSkSurface() const {
  if (!surface_)
    surface_ = CreateSkSurface();
  return surface_.get();
}

void CanvasResourceProvider::InitializePaintCanvas() {
  // Since canvas_ has a reference to canvas_image_provider_, canvas must be
  // deleted before the image_provider.
  canvas_ = nullptr;
  canvas_image_provider_ = nullptr;

  // Create an ImageDecodeCache for half float images only if the canvas is
  // using half float back storage.
  cc::ImageDecodeCache* cache_f16 = nullptr;
  if (ColorParams().GetSkColorType() == kRGBA_F16_SkColorType)
    cache_f16 = ImageDecodeCacheF16();
  canvas_image_provider_ = std::make_unique<CanvasImageProvider>(
      ImageDecodeCacheRGBA8(), cache_f16, gfx::ColorSpace::CreateSRGB(),
      color_params_.GetSkColorType(), use_hardware_decode_cache());

  cc::SkiaPaintCanvas::ContextFlushes context_flushes;
  if (IsAccelerated() &&
      !ContextProviderWrapper()
           ->ContextProvider()
           ->GetGpuFeatureInfo()
           .IsWorkaroundEnabled(gpu::DISABLE_2D_CANVAS_AUTO_FLUSH)) {
    context_flushes.enable =
        canvas_heuristic_parameters::kEnableGrContextFlushes;
    context_flushes.max_draws_before_flush =
        canvas_heuristic_parameters::kMaxDrawsBeforeContextFlush;
  }
  canvas_ = std::make_unique<cc::SkiaPaintCanvas>(GetSkSurface()->getCanvas(),
                                                  canvas_image_provider_.get(),
                                                  context_flushes);
}

cc::PaintCanvas* CanvasResourceProvider::Canvas() {
  WillDraw();

  if (!canvas_) {
    TRACE_EVENT0("blink", "CanvasResourceProvider::Canvas");
    DCHECK(!canvas_image_provider_);
    InitializePaintCanvas();
  }
  return canvas_.get();
}

void CanvasResourceProvider::OnContextDestroyed() {
  if (canvas_image_provider_) {
    DCHECK(canvas_);
    canvas_->reset_image_provider();
    canvas_image_provider_.reset();
  }
}

void CanvasResourceProvider::ReleaseLockedImages() {
  if (canvas_image_provider_)
    canvas_image_provider_->ReleaseLockedImages();
}

scoped_refptr<StaticBitmapImage> CanvasResourceProvider::SnapshotInternal() {
  if (!IsValid())
    return nullptr;

  auto paint_image = MakeImageSnapshot();
  if (paint_image.GetSkImage()->isTextureBacked() && ContextProviderWrapper()) {
    return StaticBitmapImage::Create(paint_image.GetSkImage(),
                                     ContextProviderWrapper());
  }
  return StaticBitmapImage::Create(std::move(paint_image));
}

cc::PaintImage CanvasResourceProvider::MakeImageSnapshot() {
  auto sk_image = GetSkSurface()->makeImageSnapshot();
  if (!sk_image)
    return cc::PaintImage();

  auto last_snapshot_sk_image_id = snapshot_sk_image_id_;
  snapshot_sk_image_id_ = sk_image->uniqueID();

  // Ensure that a new PaintImage::ContentId is used only when the underlying
  // SkImage changes. This is necessary to ensure that the same image results
  // in a cache hit in cc's ImageDecodeCache.
  if (snapshot_paint_image_content_id_ == PaintImage::kInvalidContentId ||
      last_snapshot_sk_image_id != snapshot_sk_image_id_) {
    snapshot_paint_image_content_id_ = PaintImage::GetNextContentId();
  }

  return PaintImageBuilder::WithDefault()
      .set_id(snapshot_paint_image_id_)
      .set_image(std::move(sk_image), snapshot_paint_image_content_id_)
      .TakePaintImage();
}

gpu::gles2::GLES2Interface* CanvasResourceProvider::ContextGL() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->ContextGL();
}

GrContext* CanvasResourceProvider::GetGrContext() const {
  if (!context_provider_wrapper_)
    return nullptr;
  return context_provider_wrapper_->ContextProvider()->GetGrContext();
}

void CanvasResourceProvider::FlushSkia() const {
  GetSkSurface()->flush();
}

bool CanvasResourceProvider::IsGpuContextLost() const {
  auto* gl = ContextGL();
  return !gl || gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR;
}

bool CanvasResourceProvider::WritePixels(const SkImageInfo& orig_info,
                                         const void* pixels,
                                         size_t row_bytes,
                                         int x,
                                         int y) {
  TRACE_EVENT0("blink", "CanvasResourceProvider::WritePixels");

  DCHECK(IsValid());
  return GetSkSurface()->getCanvas()->writePixels(orig_info, pixels, row_bytes,
                                                  x, y);
}

void CanvasResourceProvider::Clear() {
  // Clear the background transparent or opaque, as required. It would be nice
  // if this wasn't required, but the canvas is currently filled with the magic
  // transparency color. Can we have another way to manage this?
  DCHECK(IsValid());
  if (color_params_.GetOpacityMode() == kOpaque)
    Canvas()->clear(SK_ColorBLACK);
  else
    Canvas()->clear(SK_ColorTRANSPARENT);
}

uint32_t CanvasResourceProvider::ContentUniqueID() const {
  return GetSkSurface()->generationID();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::CreateResource() {
  // Needs to be implemented in subclasses that use resource recycling.
  NOTREACHED();
  return nullptr;
}

cc::ImageDecodeCache* CanvasResourceProvider::ImageDecodeCacheRGBA8() {

  if (use_hardware_decode_cache()) {
    return context_provider_wrapper_->ContextProvider()->ImageDecodeCache(
        kN32_SkColorType);
  }

  return &Image::SharedCCDecodeCache(kN32_SkColorType);
}

cc::ImageDecodeCache* CanvasResourceProvider::ImageDecodeCacheF16() {

  if (use_hardware_decode_cache()) {
    return context_provider_wrapper_->ContextProvider()->ImageDecodeCache(
        kRGBA_F16_SkColorType);
  }
  return &Image::SharedCCDecodeCache(kRGBA_F16_SkColorType);
}

void CanvasResourceProvider::RecycleResource(
    scoped_refptr<CanvasResource> resource) {
  // Need to check HasOneRef() because if there are outstanding references to
  // the resource, it cannot be safely recycled.
  if (resource->HasOneRef() && resource_recycling_enabled_ &&
      !is_single_buffered_)
    canvas_resources_.push_back(std::move(resource));
}

void CanvasResourceProvider::SetResourceRecyclingEnabled(bool value) {
  resource_recycling_enabled_ = value;
  if (!resource_recycling_enabled_)
    ClearRecycledResources();
}

void CanvasResourceProvider::ClearRecycledResources() {
  canvas_resources_.clear();
}

scoped_refptr<CanvasResource> CanvasResourceProvider::NewOrRecycledResource() {
  if (canvas_resources_.IsEmpty())
    canvas_resources_.push_back(CreateResource());

  if (IsSingleBuffered()) {
    DCHECK_EQ(canvas_resources_.size(), 1u);
    return canvas_resources_.back();
  }

  scoped_refptr<CanvasResource> resource = std::move(canvas_resources_.back());
  canvas_resources_.pop_back();
  return resource;
}

void CanvasResourceProvider::TryEnableSingleBuffering() {
  if (IsSingleBuffered() || !SupportsSingleBuffering())
    return;
  is_single_buffered_ = true;
  ClearRecycledResources();
}

bool CanvasResourceProvider::ImportResource(
    scoped_refptr<CanvasResource> resource) {
  if (!IsSingleBuffered() || !SupportsSingleBuffering())
    return false;
  canvas_resources_.clear();
  canvas_resources_.push_back(std::move(resource));
  return true;
}

}  // namespace blink
