// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_BASE_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_BASE_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/viz/service/display/skia_output_surface.h"
#include "components/viz/service/viz_service_export.h"
#include "third_party/skia/include/core/SkYUVAIndex.h"

namespace viz {

struct ImageContext;

class VIZ_SERVICE_EXPORT SkiaOutputSurfaceBase : public SkiaOutputSurface {
 public:
  // OutputSurface implementation:
  void BindToClient(OutputSurfaceClient* client) override;
  void BindFramebuffer() override;
  void SetDrawRectangle(const gfx::Rect& draw_rectangle) override;
  void SwapBuffers(OutputSurfaceFrame frame) override;
  uint32_t GetFramebufferCopyTextureFormat() override;
  bool IsDisplayedAsOverlayPlane() const override;
  unsigned GetOverlayTextureId() const override;
  gfx::BufferFormat GetOverlayBufferFormat() const override;
  bool HasExternalStencilTest() const override;
  void ApplyExternalStencil() override;
  unsigned UpdateGpuFence() override;
  void SetNeedsSwapSizeNotifications(
      bool needs_swap_size_notifications) override;
  void SetUpdateVSyncParametersCallback(
      UpdateVSyncParametersCallback callback) override;
  void SetDisplayTransformHint(gfx::OverlayTransform transform) override;
  gfx::OverlayTransform GetDisplayTransform() override;

  // SkiaOutputSurface implementation:
  void AddContextLostObserver(ContextLostObserver* observer) override;
  void RemoveContextLostObserver(ContextLostObserver* observer) override;

 protected:
  SkiaOutputSurfaceBase();
  ~SkiaOutputSurfaceBase() override;

  void PrepareYUVATextureIndices(const std::vector<ResourceMetadata>& metadatas,
                                 bool has_alpha,
                                 SkYUVAIndex indices[4]);
  void ContextLost();

  OutputSurfaceClient* client_ = nullptr;
  bool needs_swap_size_notifications_ = false;

  // Cached promise image.
  base::flat_map<ResourceId, std::unique_ptr<ImageContext>>
      promise_image_cache_;

  // Images for current frame or render pass.
  std::vector<ImageContext*> images_in_current_paint_;

  THREAD_CHECKER(thread_checker_);

 private:
  // Observers for context lost.
  base::ObserverList<ContextLostObserver>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(SkiaOutputSurfaceBase);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_EMBEDDER_SKIA_OUTPUT_SURFACE_BASE_H_
