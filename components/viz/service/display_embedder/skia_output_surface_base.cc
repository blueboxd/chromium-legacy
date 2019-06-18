// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_surface_base.h"

#include "build/build_config.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display/resource_metadata.h"
#include "components/viz/service/display_embedder/image_context.h"
#include "components/viz/service/gl/gpu_service_impl.h"
#include "ui/gl/gl_bindings.h"

namespace viz {

SkiaOutputSurfaceBase::SkiaOutputSurfaceBase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

SkiaOutputSurfaceBase::~SkiaOutputSurfaceBase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void SkiaOutputSurfaceBase::BindToClient(OutputSurfaceClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void SkiaOutputSurfaceBase::BindFramebuffer() {
  // TODO(penghuang): remove this method when GLRenderer is removed.
}

void SkiaOutputSurfaceBase::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // This GLSurface::SetDrawRectangle is a no-op for all GLSurface subclasses
  // except DirectCompositionSurfaceWin.
#if defined(OS_WIN)
  NOTIMPLEMENTED();
#endif
}

void SkiaOutputSurfaceBase::SwapBuffers(OutputSurfaceFrame frame) {
  NOTREACHED();
}

uint32_t SkiaOutputSurfaceBase::GetFramebufferCopyTextureFormat() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return GL_RGB;
}

bool SkiaOutputSurfaceBase::IsDisplayedAsOverlayPlane() const {
  return false;
}

unsigned SkiaOutputSurfaceBase::GetOverlayTextureId() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return 0;
}

gfx::BufferFormat SkiaOutputSurfaceBase::GetOverlayBufferFormat() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return gfx::BufferFormat::RGBX_8888;
}

bool SkiaOutputSurfaceBase::HasExternalStencilTest() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return false;
}

void SkiaOutputSurfaceBase::ApplyExternalStencil() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

unsigned SkiaOutputSurfaceBase::UpdateGpuFence() {
  return 0;
}

void SkiaOutputSurfaceBase::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  needs_swap_size_notifications_ = needs_swap_size_notifications;
}

void SkiaOutputSurfaceBase::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {}

void SkiaOutputSurfaceBase::SetDisplayTransformHint(
    gfx::OverlayTransform transform) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(khushalsagar): Apply this transform if rendering using Vulkan.
}

gfx::OverlayTransform SkiaOutputSurfaceBase::GetDisplayTransform() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  return gfx::OVERLAY_TRANSFORM_NONE;
}

void SkiaOutputSurfaceBase::AddContextLostObserver(
    ContextLostObserver* observer) {
  observers_.AddObserver(observer);
}

void SkiaOutputSurfaceBase::RemoveContextLostObserver(
    ContextLostObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SkiaOutputSurfaceBase::PrepareYUVATextureIndices(
    const std::vector<ResourceMetadata>& metadatas,
    bool has_alpha,
    SkYUVAIndex indices[4]) {
  DCHECK((has_alpha && (metadatas.size() == 3 || metadatas.size() == 4)) ||
         (!has_alpha && (metadatas.size() == 2 || metadatas.size() == 3)));

  bool uv_interleaved =
      has_alpha ? metadatas.size() == 3 : metadatas.size() == 2;

  indices[SkYUVAIndex::kY_Index].fIndex = 0;
  indices[SkYUVAIndex::kY_Index].fChannel = SkColorChannel::kR;

  if (uv_interleaved) {
    indices[SkYUVAIndex::kU_Index].fIndex = 1;
    indices[SkYUVAIndex::kU_Index].fChannel = SkColorChannel::kR;

    indices[SkYUVAIndex::kV_Index].fIndex = 1;
    indices[SkYUVAIndex::kV_Index].fChannel = SkColorChannel::kG;

    indices[SkYUVAIndex::kA_Index].fIndex = has_alpha ? 2 : -1;
    indices[SkYUVAIndex::kA_Index].fChannel = SkColorChannel::kR;
  } else {
    indices[SkYUVAIndex::kU_Index].fIndex = 1;
    indices[SkYUVAIndex::kU_Index].fChannel = SkColorChannel::kR;

    indices[SkYUVAIndex::kV_Index].fIndex = 2;
    indices[SkYUVAIndex::kV_Index].fChannel = SkColorChannel::kR;

    indices[SkYUVAIndex::kA_Index].fIndex = has_alpha ? 3 : -1;
    indices[SkYUVAIndex::kA_Index].fChannel = SkColorChannel::kR;
  }
}

void SkiaOutputSurfaceBase::ContextLost() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  for (auto& observer : observers_)
    observer.OnContextLost();
}

}  // namespace viz
