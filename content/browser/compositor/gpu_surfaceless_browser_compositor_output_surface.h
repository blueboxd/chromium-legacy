// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
#define CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "components/viz/service/display_embedder/buffer_queue.h"
#include "content/browser/compositor/gpu_browser_compositor_output_surface.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/ipc/common/surface_handle.h"

namespace gpu {
class GpuMemoryBufferManager;
}

namespace content {

class GpuSurfacelessBrowserCompositorOutputSurface
    : public GpuBrowserCompositorOutputSurface,
      public viz::BufferQueue::SyncTokenProvider {
 public:
  GpuSurfacelessBrowserCompositorOutputSurface(
      scoped_refptr<viz::ContextProviderCommandBuffer> context,
      gpu::SurfaceHandle surface_handle,
      gfx::BufferFormat format,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager);
  ~GpuSurfacelessBrowserCompositorOutputSurface() override;

  // viz::OutputSurface implementation.
  void SwapBuffers(viz::OutputSurfaceFrame frame) override;
  void BindFramebuffer() override;
  gfx::Rect GetCurrentFramebufferDamage() const override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  void Reshape(const gfx::Size& size,
               float device_scale_factor,
               const gfx::ColorSpace& color_space,
               bool has_alpha,
               bool use_stencil) override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  unsigned UpdateGpuFence() override;

  void SetDrawRectangle(const gfx::Rect& damage) override;

  // BrowserCompositorOutputSurface implementation.
  void OnGpuSwapBuffersCompleted(
      std::vector<ui::LatencyInfo> latency_info,
      const gpu::SwapBuffersCompleteParams& params) override;

  // BufferQueue::SyncTokenProvider implementation.
  gpu::SyncToken GenSyncToken() override;

 private:
  gfx::Size reshape_size_;
  gfx::Size swap_size_;
  bool use_gpu_fence_;
  unsigned gpu_fence_id_ = 0u;

  std::unique_ptr<viz::BufferQueue> buffer_queue_;
  unsigned current_texture_ = 0u;
  const unsigned texture_target_ = 0u;
  unsigned fbo_ = 0u;

  bool use_stencil_ = false;
  unsigned stencil_buffer_ = 0u;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_GPU_SURFACELESS_BROWSER_COMPOSITOR_OUTPUT_SURFACE_H_
