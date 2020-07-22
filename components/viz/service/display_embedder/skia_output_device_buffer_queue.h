// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/cancelable_callback.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "components/viz/service/display_embedder/output_presenter.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "components/viz/service/viz_service_export.h"

namespace viz {

class SkiaOutputSurfaceDependency;

class VIZ_SERVICE_EXPORT SkiaOutputDeviceBufferQueue : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceBufferQueue(
      std::unique_ptr<OutputPresenter> presenter,
      SkiaOutputSurfaceDependency* deps,
      gpu::MemoryTracker* memory_tracker,
      const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback);

  ~SkiaOutputDeviceBufferQueue() override;

  SkiaOutputDeviceBufferQueue(const SkiaOutputDeviceBufferQueue&) = delete;
  SkiaOutputDeviceBufferQueue& operator=(const SkiaOutputDeviceBufferQueue&) =
      delete;

  // SkiaOutputDevice overrides.
  void PreGrContextSubmit() override;
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

  bool IsPrimaryPlaneOverlay() const override;
  void SchedulePrimaryPlane(
      const base::Optional<
          OverlayProcessorInterface::OutputSurfaceOverlayPlane>& plane)
      override;
  void ScheduleOverlays(SkiaOutputSurface::OverlayList overlays) override;

 private:
  friend class SkiaOutputDeviceBufferQueueTest;

  class OverlayDataComparator {
   public:
    bool operator()(const OutputPresenter::OverlayData& a,
                    const OutputPresenter::OverlayData& b) const;
  };
  using CancelableSwapCompletionCallback =
      base::CancelableOnceCallback<void(gfx::SwapCompletionResult)>;

  OutputPresenter::Image* GetNextImage();
  void PageFlipComplete(OutputPresenter::Image* image);
  void FreeAllSurfaces();
  // Used as callback for SwapBuffersAsync and PostSubBufferAsync to finish
  // operation
  void DoFinishSwapBuffers(const gfx::Size& size,
                           std::vector<ui::LatencyInfo> latency_info,
                           const base::WeakPtr<OutputPresenter::Image>& image,
                           std::vector<OutputPresenter::OverlayData> overlays,
                           gfx::SwapCompletionResult result);

  std::unique_ptr<OutputPresenter> presenter_;

  SkiaOutputSurfaceDependency* const dependency_;
  // Format of images
  gfx::ColorSpace color_space_;
  gfx::Size image_size_;

  // All allocated images.
  std::vector<std::unique_ptr<OutputPresenter::Image>> images_;
  // This image is currently used by Skia as RenderTarget. This may be nullptr
  // if there is no drawing for the current frame or if allocation failed.
  OutputPresenter::Image* current_image_ = nullptr;
  // The last image submitted for presenting.
  OutputPresenter::Image* submitted_image_ = nullptr;
  // The image currently on the screen, if any.
  OutputPresenter::Image* displayed_image_ = nullptr;
  // These are free for use, and are not nullptr.
  base::circular_deque<OutputPresenter::Image*> available_images_;
  // These cancelable callbacks bind images that have been scheduled to display
  // but are not displayed yet. This deque will be cleared when represented
  // frames are destroyed. Use CancelableOnceCallback to prevent resources
  // from being destructed outside SkiaOutputDeviceBufferQueue life span.
  base::circular_deque<std::unique_ptr<CancelableSwapCompletionCallback>>
      swap_completion_callbacks_;
  // Scheduled overlays for the next SwapBuffers call.
  std::vector<OutputPresenter::OverlayData> pending_overlays_;
  // Committed overlays for the last SwapBuffers call.
  std::vector<OutputPresenter::OverlayData> committed_overlays_;
  // Overlays that have been returned via DoFinishSwapBuffers, but still are
  // in use by the system's WindowServer. This is only ever non-empty on macOS.
  base::flat_set<OutputPresenter::OverlayData, OverlayDataComparator>
      in_use_by_window_server_overlays_;
  // Set to true if no image is to be used for the primary plane of this frame.
  bool current_frame_has_no_primary_plane_ = false;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_BUFFER_QUEUE_H_
