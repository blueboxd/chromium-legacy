// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device.h"

#include <utility>

#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/presentation_feedback.h"

namespace viz {

SkiaOutputDevice::SkiaOutputDevice(
    bool need_swap_semaphore,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : need_swap_semaphore_(need_swap_semaphore),
      did_swap_buffer_complete_callback_(did_swap_buffer_complete_callback) {}

SkiaOutputDevice::~SkiaOutputDevice() = default;

void SkiaOutputDevice::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  NOTREACHED();
  StartSwapBuffers(std::move(feedback));
  FinishSwapBuffers(gfx::SwapResult::SWAP_FAILED, gfx::Size(),
                    std::move(latency_info));
}

void SkiaOutputDevice::SetDrawRectangle(const gfx::Rect& draw_rectangle) {}

void SkiaOutputDevice::StartSwapBuffers(
    base::Optional<BufferPresentedCallback> feedback) {
  DCHECK(!feedback_);
  DCHECK(!params_);

  feedback_ = std::move(feedback);
  params_.emplace();
  params_->swap_response.swap_id = ++swap_id_;
  params_->swap_response.timings.swap_start = base::TimeTicks::Now();
}

void SkiaOutputDevice::FinishSwapBuffers(
    gfx::SwapResult result,
    const gfx::Size& size,
    std::vector<ui::LatencyInfo> latency_info) {
  DCHECK(params_);

  params_->swap_response.result = result;
  params_->swap_response.timings.swap_end = base::TimeTicks::Now();
  did_swap_buffer_complete_callback_.Run(*params_, size);

  if (feedback_) {
    std::move(*feedback_)
        .Run(gfx::PresentationFeedback(
            params_->swap_response.timings.swap_start,
            base::TimeDelta() /* interval */,
            params_->swap_response.result == gfx::SwapResult::SWAP_ACK
                ? 0
                : gfx::PresentationFeedback::Flags::kFailure));
  }

  feedback_.reset();
  auto& response = params_->swap_response;

  for (auto& latency : latency_info) {
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_GPU_SWAP_BUFFER_COMPONENT, response.timings.swap_start);
    latency.AddLatencyNumberWithTimestamp(
        ui::INPUT_EVENT_LATENCY_FRAME_SWAP_COMPONENT,
        response.timings.swap_end);
  }
  latency_tracker_.OnGpuSwapBuffersCompleted(latency_info);

  params_.reset();
}

void SkiaOutputDevice::EnsureBackbuffer() {}
void SkiaOutputDevice::DiscardBackbuffer() {}

gl::GLImage* SkiaOutputDevice::GetOverlayImage() {
  return nullptr;
}

std::unique_ptr<gfx::GpuFence> SkiaOutputDevice::SubmitOverlayGpuFence() {
  return nullptr;
}

}  // namespace viz
