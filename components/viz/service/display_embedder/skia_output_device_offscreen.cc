// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_offscreen.h"

#include <utility>

#include "third_party/skia/include/core/SkSurface.h"

namespace viz {

SkiaOutputDeviceOffscreen::SkiaOutputDeviceOffscreen(
    GrContext* gr_context,
    bool flipped,
    bool has_alpha,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(false /*need_swap_semaphore */,
                       did_swap_buffer_complete_callback),
      gr_context_(gr_context),
      has_alpha_(has_alpha) {
  capabilities_.flipped_output_surface = flipped;
  capabilities_.supports_post_sub_buffer = true;
}

SkiaOutputDeviceOffscreen::~SkiaOutputDeviceOffscreen() = default;

void SkiaOutputDeviceOffscreen::Reshape(const gfx::Size& size,
                                        float device_scale_factor,
                                        const gfx::ColorSpace& color_space,
                                        bool has_alpha,
                                        gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  // Some Vulkan drivers do not support kRGB_888x_SkColorType. Always use
  // kRGBA_8888_SkColorType instead and initialize surface to opaque alpha.
  image_info_ =
      SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                        has_alpha_ ? kPremul_SkAlphaType : kOpaque_SkAlphaType,
                        color_space.ToSkColorSpace());
  sk_surface_ = SkSurface::MakeRenderTarget(
      gr_context_, SkBudgeted::kNo, image_info_, 0 /* sampleCount */,
      capabilities_.flipped_output_surface ? kTopLeft_GrSurfaceOrigin
                                           : kBottomLeft_GrSurfaceOrigin,
      nullptr /* surfaceProps */);
  DCHECK(!!sk_surface_);

  if (!has_alpha_) {
    is_emulated_rgbx_ = true;
    // Initialize alpha channel to opaque.
    auto* canvas = sk_surface_->getCanvas();
    canvas->clear(SkColorSetARGB(255, 0, 0, 0));
  }
}

void SkiaOutputDeviceOffscreen::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  // Reshape should have been called first.
  DCHECK(sk_surface_);

  StartSwapBuffers(std::move(feedback));
  FinishSwapBuffers(gfx::SwapResult::SWAP_ACK,
                    gfx::Size(sk_surface_->width(), sk_surface_->height()),
                    std::move(latency_info));
}

void SkiaOutputDeviceOffscreen::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  return SwapBuffers(std::move(feedback), std::move(latency_info));
}

void SkiaOutputDeviceOffscreen::EnsureBackbuffer() {
  if (!image_info_.isEmpty() && !sk_surface_) {
    sk_surface_ = SkSurface::MakeRenderTarget(
        gr_context_, SkBudgeted::kNo, image_info_, 0 /* sampleCount */,
        capabilities_.flipped_output_surface ? kTopLeft_GrSurfaceOrigin
                                             : kBottomLeft_GrSurfaceOrigin,
        nullptr /* surfaceProps */);
  }
}

void SkiaOutputDeviceOffscreen::DiscardBackbuffer() {
  sk_surface_.reset();
}

SkSurface* SkiaOutputDeviceOffscreen::BeginPaint() {
  return sk_surface_.get();
}

void SkiaOutputDeviceOffscreen::EndPaint(const GrBackendSemaphore& semaphore) {}

}  // namespace viz
