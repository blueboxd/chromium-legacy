// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_OFFSCREEN_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_OFFSCREEN_H_

#include "base/macros.h"
#include "components/viz/service/display_embedder/skia_output_device.h"
#include "third_party/skia/include/core/SkImageInfo.h"

class GrContext;

namespace viz {

class SkiaOutputDeviceOffscreen : public SkiaOutputDevice {
 public:
  SkiaOutputDeviceOffscreen(
      GrContext* gr_context,
      bool flipped,
      bool has_alpha,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  ~SkiaOutputDeviceOffscreen() override;

  // SkiaOutputDevice implementation:
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               gfx::OverlayTransform transform) override;
  void SwapBuffers(BufferPresentedCallback feedback,
                   std::vector<ui::LatencyInfo> latency_info) override;
  void PostSubBuffer(const gfx::Rect& rect,
                     BufferPresentedCallback feedback,
                     std::vector<ui::LatencyInfo> latency_info) override;
  void EnsureBackbuffer() override;
  void DiscardBackbuffer() override;
  SkSurface* BeginPaint() override;
  void EndPaint(const GrBackendSemaphore& semaphore) override;

 protected:
  GrContext* const gr_context_;
  const bool has_alpha_;
  sk_sp<SkSurface> sk_surface_;
  bool supports_rgbx_ = true;

 private:
  SkImageInfo image_info_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDeviceOffscreen);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_OFFSCREEN_H_
