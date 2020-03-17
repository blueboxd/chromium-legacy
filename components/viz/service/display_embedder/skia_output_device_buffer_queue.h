// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_

#include "base/cancelable_callback.h"
#include "base/macros.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/viz_service_export.h"
#include "gpu/command_buffer/service/shared_image_factory.h"

namespace gl {
class GLSurface;
}  // namespace gl

namespace viz {

class SkiaOutputSurfaceDependency;

class VIZ_SERVICE_EXPORT SkiaOutputDeviceBufferQueue final
    : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceBufferQueue(
      scoped_refptr<gl::GLSurface> gl_surface,
      SkiaOutputSurfaceDependency* deps,
      gpu::MemoryTracker* memory_tracker,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback);
  SkiaOutputDeviceBufferQueue(
      scoped_refptr<gl::GLSurface> gl_surface,
      SkiaOutputSurfaceDependency* deps,
      gpu::MemoryTracker* memory_tracker,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback,
      uint32_t shared_image_usage);
  ~SkiaOutputDeviceBufferQueue() override;

  static std::unique_ptr<SkiaOutputDeviceBufferQueue> Create(
      SkiaOutputSurfaceDependency* deps,
      gpu::MemoryTracker* memory_tracker,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback);

  void SwapBuffers(BufferPresentedCallback feedback,
                   std::vector<ui::LatencyInfo> latency_info) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     std::vector<ui::LatencyInfo> latency_info) override;
  void CommitOverlayPlanes(BufferPresentedCallback feedback,
                           std::vector<ui::LatencyInfo> latency_info) override;
  bool Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               gfx::BufferFormat format,
               gfx::OverlayTransform transform) override;
  SkSurface* BeginPaint(
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndPaint() override;
  bool supports_alpha() { return true; }

  bool IsPrimaryPlaneOverlay() const override;
  void SchedulePrimaryPlane(
      const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane)
      override;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays) override;

  gl::GLSurface* gl_surface() { return gl_surface_.get(); }

 private:
  friend class SkiaOutputDeviceBufferQueueTest;
  class Image;
  class OverlayData;

  using CancelableSwapCompletionCallback =
      base::CancelableOnceCallback<void(gfx::SwapResult,
                                        std::unique_ptr<gfx::GpuFence>)>;

  Image* GetNextImage();
  void PageFlipComplete(Image* image);
  void FreeAllSurfaces();
  // Used as callback for SwapBuff ersAsync and PostSubBufferAsync to finish
  // operation
  void DoFinishSwapBuffers(const gfx::Size& size,
                           std::vector<ui::LatencyInfo> latency_info,
                           const base::WeakPtr<Image>& image,
                           std::vector<OverlayData> overlays,
                           gfx::SwapResult result,
                           std::unique_ptr<gfx::GpuFence> gpu_fence);

  SkiaOutputSurfaceDependency* const dependency_;
  scoped_refptr<gl::GLSurface> gl_surface_;
  const bool supports_async_swap_;
  // Format of images
  gfx::ColorSpace color_space_;
  gfx::Size image_size_;
  ResourceFormat image_format_;

  // All allocated images.
  std::vector<std::unique_ptr<Image>> images_;
  // This image is currently used by Skia as RenderTarget. This may be nullptr
  // if there is no drawing for the current frame or if allocation failed.
  Image* current_image_ = nullptr;
  // The last image submitted for presenting.
  Image* submitted_image_ = nullptr;
  // The image currently on the screen, if any.
  Image* displayed_image_ = nullptr;
  // These are free for use, and are not nullptr.
  base::circular_deque<Image*> available_images_;
  // These cancelable callbacks bind images that have been scheduled to display
  // but are not displayed yet. This deque will be cleared when represented
  // frames are destroyed. Use CancelableOnceCallback to prevent resources
  // from being destructed outside SkiaOutputDeviceBufferQueue life span.
  base::circular_deque<std::unique_ptr<CancelableSwapCompletionCallback>>
      swap_completion_callbacks_;
  // Scheduled overlays for the next SwapBuffers call.
  std::vector<OverlayData> pending_overlays_;
  // Committed overlays for the last SwapBuffers call.
  std::vector<OverlayData> committed_overlays_;

  // Shared Image factories
  gpu::SharedImageFactory shared_image_factory_;
  std::unique_ptr<gpu::SharedImageRepresentationFactory>
      shared_image_representation_factory_;
  uint32_t shared_image_usage_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceBufferQueue);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
