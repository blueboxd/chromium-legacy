// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device.h"

#include <utility>

#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/presentation_feedback.h"

namespace viz {

SkiaOutputDevice::SkiaOutputDevice(
    bool need_swap_semaphore,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : need_swap_semaphore_(need_swap_semaphore),
      did_swap_buffer_complete_callback_(did_swap_buffer_complete_callback) {}

SkiaOutputDevice::~SkiaOutputDevice() = default;

gfx::SwapResponse SkiaOutputDevice::PostSubBuffer(
    const gfx::Rect& rect,
    const GrBackendSemaphore& semaphore,
    BufferPresentedCallback feedback) {
  NOTREACHED();
  StartSwapBuffers(std::move(feedback));
  return FinishSwapBuffers(gfx::SwapResult::SWAP_FAILED);
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

gfx::SwapResponse SkiaOutputDevice::FinishSwapBuffers(gfx::SwapResult result) {
  DCHECK(params_);

  params_->swap_response.result = result;
  params_->swap_response.timings.swap_end = base::TimeTicks::Now();
  if (feedback_) {
    std::move(*feedback_)
        .Run(gfx::PresentationFeedback(
            params_->swap_response.timings.swap_start,
            base::TimeDelta() /* interval */,
            params_->swap_response.result == gfx::SwapResult::SWAP_ACK
                ? 0
                : gfx::PresentationFeedback::Flags::kFailure));
  }
  did_swap_buffer_complete_callback_.Run(
      *params_, gfx::Size(draw_surface_->width(), draw_surface_->height()));

  feedback_.reset();
  auto response = params_->swap_response;
  params_.reset();
  return response;
}

void SkiaOutputDevice::EnsureBackbuffer() {}
void SkiaOutputDevice::DiscardBackbuffer() {}

}  // namespace viz
