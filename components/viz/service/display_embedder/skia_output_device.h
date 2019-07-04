// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "components/viz/service/display/output_surface.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/swap_result.h"

class GrBackendSemaphore;
class SkSurface;

namespace gfx {
class ColorSpace;
class Rect;
class Size;
struct PresentationFeedback;
}  // namespace gfx

namespace viz {

class SkiaOutputDevice {
 public:
  using BufferPresentedCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback& feedback)>;
  using DidSwapBufferCompleteCallback =
      base::RepeatingCallback<void(gpu::SwapBuffersCompleteParams,
                                   const gfx::Size& pixel_size)>;
  SkiaOutputDevice(
      bool need_swap_semaphore,
      DidSwapBufferCompleteCallback did_swap_buffer_complete_callback);
  virtual ~SkiaOutputDevice();

  // SkSurface that can be drawn to.
  SkSurface* draw_surface() const { return draw_surface_.get(); }

  // Changes the size of draw surface and invalidates it's contents.
  virtual void Reshape(const gfx::Size& size,
                       float device_scale_factor,
                       const gfx::ColorSpace& color_space,
                       bool has_alpha) = 0;

  // Presents DrawSurface.
  virtual gfx::SwapResponse SwapBuffers(const GrBackendSemaphore& semaphore,
                                        BufferPresentedCallback feedback) = 0;
  virtual gfx::SwapResponse PostSubBuffer(const gfx::Rect& rect,
                                          const GrBackendSemaphore& semaphore,
                                          BufferPresentedCallback feedback);

  // Set the rectangle that will be drawn into on the surface.
  virtual void SetDrawRectangle(const gfx::Rect& draw_rectangle);

  const OutputSurface::Capabilities& capabilities() const {
    return capabilities_;
  }

  // EnsureBackbuffer called when output surface is visible and may be drawn to.
  // DiscardBackbuffer called when output surface is hidden and will not be
  // drawn to. Default no-op.
  virtual void EnsureBackbuffer();
  virtual void DiscardBackbuffer();

  bool need_swap_semaphore() const { return need_swap_semaphore_; }

 protected:
  void StartSwapBuffers(base::Optional<BufferPresentedCallback> feedback);
  gfx::SwapResponse FinishSwapBuffers(gfx::SwapResult result);

  sk_sp<SkSurface> draw_surface_;
  OutputSurface::Capabilities capabilities_;

 private:
  const bool need_swap_semaphore_;
  uint64_t swap_id_ = 0;
  DidSwapBufferCompleteCallback did_swap_buffer_complete_callback_;

  // Only valid between StartSwapBuffers and FinishSwapBuffers.
  base::Optional<BufferPresentedCallback> feedback_;
  base::Optional<gpu::SwapBuffersCompleteParams> params_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputDevice);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_DEVICE_H_
