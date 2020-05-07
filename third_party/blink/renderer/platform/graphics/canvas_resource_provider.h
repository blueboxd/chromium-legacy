// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CANVAS_RESOURCE_PROVIDER_H_

#include "cc/paint/skia_paint_canvas.h"
#include "cc/raster/playback_image_provider.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource.h"
#include "third_party/blink/renderer/platform/graphics/image_orientation.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/skia/include/core/SkSurface.h"

class GrContext;

namespace cc {
class ImageDecodeCache;
class PaintCanvas;
}  // namespace cc

namespace gpu {
namespace gles2 {

class GLES2Interface;

}  // namespace gles2

namespace raster {

class RasterInterface;

}  // namespace raster
}  // namespace gpu

namespace blink {

class CanvasResourceDispatcher;
class WebGraphicsContext3DProviderWrapper;

// CanvasResourceProvider
//==============================================================================
//
// This is an abstract base class that encapsulates a drawable graphics
// resource.  Subclasses manage specific resource types (Gpu Textures,
// GpuMemoryBuffer, Bitmap in RAM). CanvasResourceProvider serves as an
// abstraction layer for these resource types. It is designed to serve
// the needs of Canvas2DLayerBridge, but can also be used as a general purpose
// provider of drawable surfaces for 2D rendering with skia.
//
// General usage:
//   1) Use the Create() static method to create an instance
//   2) use Canvas() to get a drawing interface
//   3) Call Snapshot() to acquire a bitmap with the rendered image in it.

class PLATFORM_EXPORT CanvasResourceProvider
    : public WebGraphicsContext3DProviderWrapper::DestructionObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // TODO(juanmihd@ bug/1035589) ResourceUsage will be removed soon, try
  // avoiding using this.
  enum class ResourceUsage {
    kSoftwareResourceUsage = 0,  // deprecated
    kSoftwareCompositedResourceUsage = 1,
    kAcceleratedResourceUsage = 2,  // deprecated
    kAcceleratedCompositedResourceUsage = 3,
    kAcceleratedDirect2DResourceUsage = 4,
    kAcceleratedDirect3DResourceUsage = 5,
    kSoftwareCompositedDirect2DResourceUsage = 6,  // deprecated
    kMaxValue = kSoftwareCompositedDirect2DResourceUsage,
  };

  // Bitmask of allowed presentation modes.
  enum : uint8_t {
    // GPU Texture or shared memory bitmap
    kDefaultPresentationMode = 0,
    // Allow CHROMIUM_image gl extension
    kAllowImageChromiumPresentationMode = 1 << 0,
    // Allow swap chains (only on Windows)
    kAllowSwapChainPresentationMode = 1 << 1,
  };

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum ResourceProviderType {
    kTexture = 0,
    kBitmap = 1,
    kSharedBitmap = 2,
    kTextureGpuMemoryBuffer = 3,
    kBitmapGpuMemoryBuffer = 4,
    kSharedImage = 5,
    kDirectGpuMemoryBuffer = 6,
    kPassThrough = 7,
    kSwapChain = 8,
    kWebGPUSharedImage = 9,
    kMaxValue = kWebGPUSharedImage,
  };

  using RestoreMatrixClipStackCb =
      base::RepeatingCallback<void(cc::PaintCanvas*)>;

  // TODO(juanmihd@ bug/1078518) Check whether SkFilterQuality is needed in all
  // of this, or just call setFilterQuality explicitly.
  static std::unique_ptr<CanvasResourceProvider> CreateBitmapProvider(
      const IntSize&,
      SkFilterQuality,
      const CanvasColorParams&);

  static std::unique_ptr<CanvasResourceProvider> CreateSharedBitmapProvider(
      const IntSize&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      SkFilterQuality,
      const CanvasColorParams&,
      base::WeakPtr<CanvasResourceDispatcher>);

  // Specifies whether the provider should rasterize paint commands on the CPU
  // or GPU. This is used to support software raster with GPU compositing.
  enum class RasterMode {
    kGPU,
    kCPU,
  };

  static std::unique_ptr<CanvasResourceProvider> CreateSharedImageProvider(
      const IntSize&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      SkFilterQuality,
      const CanvasColorParams&,
      bool is_origin_top_left,
      RasterMode raster_mode,
      uint32_t shared_image_usage_flags);

  static std::unique_ptr<CanvasResourceProvider> CreatePassThroughProvider(
      const IntSize&,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      SkFilterQuality,
      const CanvasColorParams&,
      bool is_origin_top_left,
      base::WeakPtr<CanvasResourceDispatcher>);

  // TODO(juanmihd): Clean up creation methods/usage. See crbug.com/1035589.
  static std::unique_ptr<CanvasResourceProvider> Create(
      const IntSize&,
      ResourceUsage,
      base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
      unsigned msaa_sample_count,
      SkFilterQuality,
      const CanvasColorParams&,
      uint8_t presentation_mode,
      base::WeakPtr<CanvasResourceDispatcher>,
      bool is_origin_top_left = true);

  // Use Snapshot() for capturing a frame that is intended to be displayed via
  // the compositor. Cases that are destined to be transferred via a
  // TransferableResource should call ProduceCanvasResource() instead.
  // The ImageOrientationEnum conveys the desired orientation of the image, and
  // should be derived from the source of the bitmap data.
  virtual scoped_refptr<CanvasResource> ProduceCanvasResource() = 0;
  virtual scoped_refptr<StaticBitmapImage> Snapshot(
      const ImageOrientation& = kDefaultImageOrientation) = 0;

  // WebGraphicsContext3DProvider::DestructionObserver implementation.
  void OnContextDestroyed() override;

  cc::PaintCanvas* Canvas();
  void ReleaseLockedImages();
  sk_sp<cc::PaintRecord> FlushCanvas();
  const CanvasColorParams& ColorParams() const { return color_params_; }
  void SetFilterQuality(SkFilterQuality quality) { filter_quality_ = quality; }
  const IntSize& Size() const { return size_; }
  bool IsOriginTopLeft() const { return is_origin_top_left_; }
  virtual bool IsValid() const = 0;
  virtual bool IsAccelerated() const = 0;
  // Returns true if the resource can be used by the display compositor.
  virtual bool SupportsDirectCompositing() const = 0;
  virtual bool SupportsSingleBuffering() const { return false; }
  uint32_t ContentUniqueID() const;
  CanvasResourceDispatcher* ResourceDispatcher() {
    return resource_dispatcher_.get();
  }

  // Indicates that the compositing path is single buffered, meaning that
  // ProduceCanvasResource() return a reference to the same resource each time,
  // which implies that Producing an animation frame may overwrite the resource
  // used by the previous frame. This results in graphics updates skipping the
  // queue, thus reducing latency, but with the possible side effects of tearing
  // (in cases where the resource is scanned out directly) and irregular frame
  // rate.
  bool IsSingleBuffered() const { return is_single_buffered_; }

  // Attempt to enable single buffering mode on this resource provider.  May
  // fail if the CanvasResourcePRovider subclass does not support this mode of
  // operation.
  void TryEnableSingleBuffering();

  // Only works in single buffering mode.
  bool ImportResource(scoped_refptr<CanvasResource>);

  void RecycleResource(scoped_refptr<CanvasResource>);
  void SetResourceRecyclingEnabled(bool);
  void ClearRecycledResources();
  scoped_refptr<CanvasResource> NewOrRecycledResource();

  SkSurface* GetSkSurface() const;
  bool IsGpuContextLost() const;
  bool WritePixels(const SkImageInfo& orig_info,
                   const void* pixels,
                   size_t row_bytes,
                   int x,
                   int y);

  virtual gpu::Mailbox GetBackingMailboxForOverwrite(
      MailboxSyncMode sync_mode) {
    NOTREACHED();
    return gpu::Mailbox();
  }
  virtual GLenum GetBackingTextureTarget() const { return GL_TEXTURE_2D; }
  virtual void* GetPixelBufferAddressForOverwrite() {
    NOTREACHED();
    return nullptr;
  }
  void Clear();
  ~CanvasResourceProvider() override;

  base::WeakPtr<CanvasResourceProvider> CreateWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Notifies the provider when the texture params associated with |resource|
  // are modified externally from the provider's SkSurface.
  virtual void NotifyTexParamsModified(const CanvasResource* resource) {}

  size_t cached_resources_count_for_testing() const {
    return canvas_resources_.size();
  }

  void SkipQueuedDrawCommands();
  void SetRestoreClipStackCallback(RestoreMatrixClipStackCb);
  bool needs_flush() const { return needs_flush_; }
  void RestoreBackBuffer(const cc::PaintImage&);

  ResourceProviderType GetType() const { return type_; }
  bool HasRecordedDrawOps() const;

  void OnDestroyResource();

 protected:
  class CanvasImageProvider;

  gpu::gles2::GLES2Interface* ContextGL() const;
  gpu::raster::RasterInterface* RasterInterface() const;
  GrContext* GetGrContext() const;
  base::WeakPtr<WebGraphicsContext3DProviderWrapper> ContextProviderWrapper() {
    return context_provider_wrapper_;
  }
  unsigned GetMSAASampleCount() const { return msaa_sample_count_; }
  GrSurfaceOrigin GetGrSurfaceOrigin() const {
    return is_origin_top_left_ ? kTopLeft_GrSurfaceOrigin
                               : kBottomLeft_GrSurfaceOrigin;
  }
  SkFilterQuality FilterQuality() const { return filter_quality_; }
  scoped_refptr<StaticBitmapImage> SnapshotInternal(const ImageOrientation&);
  scoped_refptr<CanvasResource> GetImportedResource() const;

  CanvasResourceProvider(const ResourceProviderType&,
                         const IntSize&,
                         unsigned msaa_sample_count,
                         SkFilterQuality,
                         const CanvasColorParams&,
                         bool is_origin_top_left,
                         base::WeakPtr<WebGraphicsContext3DProviderWrapper>,
                         base::WeakPtr<CanvasResourceDispatcher>);

  // Its important to use this method for generating PaintImage wrapped canvas
  // snapshots to get a cache hit from cc's ImageDecodeCache. This method
  // ensures that the PaintImage ID for the snapshot, used for keying
  // decodes/uploads in the cache is invalidated only when the canvas contents
  // change.
  cc::PaintImage MakeImageSnapshot();
  virtual void RasterRecord(sk_sp<cc::PaintRecord>);
  CanvasImageProvider* GetOrCreateCanvasImageProvider();

  ResourceProviderType type_;
  mutable sk_sp<SkSurface> surface_;  // mutable for lazy init
  SkSurface::ContentChangeMode mode_ = SkSurface::kRetain_ContentChangeMode;

 private:
  virtual sk_sp<SkSurface> CreateSkSurface() const = 0;
  virtual scoped_refptr<CanvasResource> CreateResource();
  bool use_hardware_decode_cache() const {
    return IsAccelerated() && context_provider_wrapper_;
  }
  // Notifies before any drawing will be done on the resource used by this
  // provider.
  virtual void WillDraw() {}

  cc::ImageDecodeCache* ImageDecodeCacheRGBA8();
  cc::ImageDecodeCache* ImageDecodeCacheF16();
  void EnsureSkiaCanvas();
  void SetNeedsFlush() { needs_flush_ = true; }

  base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper_;
  base::WeakPtr<CanvasResourceDispatcher> resource_dispatcher_;
  const IntSize size_;
  const unsigned msaa_sample_count_;
  SkFilterQuality filter_quality_;
  const CanvasColorParams color_params_;
  const bool is_origin_top_left_;
  std::unique_ptr<CanvasImageProvider> canvas_image_provider_;
  std::unique_ptr<cc::SkiaPaintCanvas> skia_canvas_;
  std::unique_ptr<PaintRecorder> recorder_;

  bool needs_flush_ = false;

  const cc::PaintImage::Id snapshot_paint_image_id_;
  cc::PaintImage::ContentId snapshot_paint_image_content_id_ =
      cc::PaintImage::kInvalidContentId;
  uint32_t snapshot_sk_image_id_ = 0u;

  // When and if |resource_recycling_enabled_| is false, |canvas_resources_|
  // will only hold one CanvasResource at most.
  WTF::Vector<scoped_refptr<CanvasResource>> canvas_resources_;
  bool resource_recycling_enabled_ = true;
  bool is_single_buffered_ = false;

  // The maximum number of in-flight resources waiting to be used for recycling.
  static constexpr int kMaxRecycledCanvasResources = 2;
  // The maximum number of draw ops executed on the canvas, after which the
  // underlying GrContext is flushed.
  static constexpr int kMaxDrawsBeforeContextFlush = 50;

  size_t num_inflight_resources_ = 0;
  size_t max_inflight_resources_ = 0;

  RestoreMatrixClipStackCb restore_clip_stack_callback_;

  base::WeakPtrFactory<CanvasResourceProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CanvasResourceProvider);
};

}  // namespace blink

#endif
