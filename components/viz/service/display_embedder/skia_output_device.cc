// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "components/viz/service/display/dc_layer_overlay.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/presentation_feedback.h"

namespace viz {

SkiaOutputDevice::ScopedPaint::ScopedPaint(SkiaOutputDevice* device)
    : device_(device), sk_surface_(device->BeginPaint(&end_semaphores_)) {
  DCHECK(sk_surface_);
}
SkiaOutputDevice::ScopedPaint::~ScopedPaint() {
  DCHECK(end_semaphores_.empty());
  device_->EndPaint();
}

SkiaOutputDevice::SkiaOutputDevice(
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : did_swap_buffer_complete_callback_(
          std::move(did_swap_buffer_complete_callback)),
      memory_type_tracker_(
          std::make_unique<gpu::MemoryTypeTracker>(memory_tracker)) {}

SkiaOutputDevice::~SkiaOutputDevice() = default;

void SkiaOutputDevice::CommitOverlayPlanes(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  NOTREACHED();
}

void SkiaOutputDevice::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  NOTREACHED();
}

void SkiaOutputDevice::SetDrawRectangle(const gfx::Rect& draw_rectangle) {}

void SkiaOutputDevice::SetGpuVSyncEnabled(bool enabled) {
  NOTIMPLEMENTED();
}

bool SkiaOutputDevice::IsPrimaryPlaneOverlay() const {
  return false;
}

void SkiaOutputDevice::SchedulePrimaryPlane(
    const OverlayProcessorInterface::OutputSurfaceOverlayPlane& plane) {
  NOTIMPLEMENTED();
}

void SkiaOutputDevice::ScheduleOverlays(
    SkiaOutputSurface::OverlayList overlays) {
  NOTIMPLEMENTED();
}

#if defined(OS_WIN)
void SkiaOutputDevice::SetEnableDCLayers(bool enable) {
  NOTIMPLEMENTED();
}
#endif

void SkiaOutputDevice::StartSwapBuffers(BufferPresentedCallback feedback) {
  DCHECK_LT(static_cast<int>(pending_swaps_.size()),
            capabilities_.max_frames_pending);

  pending_swaps_.emplace(++swap_id_, std::move(feedback));
}

void SkiaOutputDevice::FinishSwapBuffers(
    gfx::SwapResult result,
    const gfx::Size& size,
    std::vector<ui::LatencyInfo> latency_info) {
  DCHECK(!pending_swaps_.empty());

  const gpu::SwapBuffersCompleteParams& params =
      pending_swaps_.front().Complete(result);

  did_swap_buffer_complete_callback_.Run(params, size);

  pending_swaps_.front().CallFeedback();

  for (auto& latency : latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT,
        params.swap_response.timings.swap_start);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
        params.swap_response.timings.swap_end);
  }
  latency_tracker_.OnGpuSwapBuffersCompleted(latency_info);

  pending_swaps_.pop();
}

void SkiaOutputDevice::EnsureBackbuffer() {}
void SkiaOutputDevice::DiscardBackbuffer() {}

SkiaOutputDevice::SwapInfo::SwapInfo(
    uint64_t swap_id,
    SkiaOutputDevice::BufferPresentedCallback feedback)
    : feedback_(std::move(feedback)) {
  params_.swap_response.swap_id = swap_id;
  params_.swap_response.timings.swap_start = base::TimeTicks::Now();
}

SkiaOutputDevice::SwapInfo::SwapInfo(SwapInfo&& other) = default;

SkiaOutputDevice::SwapInfo::~SwapInfo() = default;

const gpu::SwapBuffersCompleteParams& SkiaOutputDevice::SwapInfo::Complete(
    gfx::SwapResult result) {
  params_.swap_response.result = result;
  params_.swap_response.timings.swap_end = base::TimeTicks::Now();

  return params_;
}

void SkiaOutputDevice::SwapInfo::CallFeedback() {
  if (feedback_) {
    uint32_t flags = 0;
    if (params_.swap_response.result != gfx::SwapResult::SWAP_ACK)
      flags = gfx::PresentationFeedback::Flags::kFailure;

    std::move(feedback_).Run(
        gfx::PresentationFeedback(params_.swap_response.timings.swap_start,
                                  /*interval=*/base::TimeDelta(), flags));
  }
}

}  // namespace viz
