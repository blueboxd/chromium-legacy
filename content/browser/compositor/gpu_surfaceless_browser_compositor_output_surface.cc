// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/gpu_surfaceless_browser_compositor_output_surface.h"

#include <utility>

#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "components/viz/service/display_embedder/buffer_queue.h"
#include "content/browser/compositor/reflector_impl.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu/command_buffer/common/gpu_memory_buffer_support.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "services/viz/public/cpp/gpu/context_provider_command_buffer.h"
#include "ui/gl/buffer_format_utils.h"
#include "ui/gl/gl_enums.h"

namespace content {

GpuSurfacelessBrowserCompositorOutputSurface::
    GpuSurfacelessBrowserCompositorOutputSurface(
        scoped_refptr<viz::ContextProviderCommandBuffer> context,
        gpu::SurfaceHandle surface_handle,
        gfx::BufferFormat format,
        gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager)
    : GpuBrowserCompositorOutputSurface(context, surface_handle),
      use_gpu_fence_(
          context_provider_->ContextCapabilities().chromium_gpu_fence &&
          context_provider_->ContextCapabilities()
              .use_gpu_fences_for_overlay_planes),
      texture_target_(gpu::GetBufferTextureTarget(
          gfx::BufferUsage::SCANOUT,
          format,
          context_provider_->ContextCapabilities())) {
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.flipped_output_surface = true;
  // Set |max_frames_pending| to 2 for surfaceless, which aligns scheduling
  // more closely with the previous surfaced behavior.
  // With a surface, swap buffer ack used to return early, before actually
  // presenting the back buffer, enabling the browser compositor to run ahead.
  // Surfaceless implementation acks at the time of actual buffer swap, which
  // shifts the start of the new frame forward relative to the old
  // implementation.
  capabilities_.max_frames_pending = 2;

  // It is safe to pass a raw pointer to *this because |buffer_queue_| is fully
  // owned and it doesn't use the SyncTokenProvider after it's destroyed.
  buffer_queue_.reset(new viz::BufferQueue(
      context->SharedImageInterface(), format, gpu_memory_buffer_manager,
      surface_handle, /*sync_token_provider=*/this));
  context_provider_->ContextGL()->GenFramebuffers(1, &fbo_);
}

GpuSurfacelessBrowserCompositorOutputSurface::
    ~GpuSurfacelessBrowserCompositorOutputSurface() {
  auto* gl = context_provider_->ContextGL();
  if (gpu_fence_id_ > 0)
    gl->DestroyGpuFenceCHROMIUM(gpu_fence_id_);
  DCHECK_NE(0u, fbo_);
  gl->DeleteFramebuffers(1, &fbo_);
  if (stencil_buffer_)
    gl->DeleteRenderbuffers(1, &stencil_buffer_);

  // Freeing the BufferQueue here ensures that *this is fully alive in case the
  // BufferQueue needs the SyncTokenProvider functionality.
  buffer_queue_.reset();
  fbo_ = 0u;
  stencil_buffer_ = 0u;
}

bool GpuSurfacelessBrowserCompositorOutputSurface::IsDisplayedAsOverlayPlane()
    const {
  return true;
}

unsigned GpuSurfacelessBrowserCompositorOutputSurface::GetOverlayTextureId()
    const {
  DCHECK(current_texture_);
  return current_texture_;
}

gfx::BufferFormat
GpuSurfacelessBrowserCompositorOutputSurface::GetOverlayBufferFormat() const {
  DCHECK(buffer_queue_);
  return buffer_queue_->buffer_format();
}

void GpuSurfacelessBrowserCompositorOutputSurface::SwapBuffers(
    viz::OutputSurfaceFrame frame) {
  DCHECK(buffer_queue_);
  DCHECK(reshape_size_ == frame.size);
  // TODO(ccameron): What if a swap comes again before OnGpuSwapBuffersCompleted
  // happens, we'd see the wrong swap size there?
  swap_size_ = reshape_size_;

  gfx::Rect damage_rect =
      frame.sub_buffer_rect ? *frame.sub_buffer_rect : gfx::Rect(swap_size_);
  context_provider_->ContextGL()->EndSharedImageAccessDirectCHROMIUM(
      current_texture_);
  buffer_queue_->SwapBuffers(damage_rect);

  GpuBrowserCompositorOutputSurface::SwapBuffers(std::move(frame));
}

void GpuSurfacelessBrowserCompositorOutputSurface::BindFramebuffer() {
  auto* gl = context_provider_->ContextGL();
  gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
  DCHECK(buffer_queue_);
  gpu::SyncToken creation_sync_token;
  const gpu::Mailbox current_buffer =
      buffer_queue_->GetCurrentBuffer(&creation_sync_token);
  if (current_buffer.IsZero())
    return;
  // TODO(andrescj): if the texture hasn't changed since the last call to
  // BindFrameBuffer(), we may be able to avoid mutating the FBO which may lead
  // to performance improvements.
  gl->WaitSyncTokenCHROMIUM(creation_sync_token.GetConstData());
  current_texture_ =
      gl->CreateAndTexStorage2DSharedImageCHROMIUM(current_buffer.name);
  gl->BeginSharedImageAccessDirectCHROMIUM(
      current_texture_, GL_SHARED_IMAGE_ACCESS_MODE_READWRITE_CHROMIUM);
  gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           texture_target_, current_texture_, 0);

#if DCHECK_IS_ON() && defined(OS_CHROMEOS)
  const GLenum result = gl->CheckFramebufferStatus(GL_FRAMEBUFFER);
  if (result != GL_FRAMEBUFFER_COMPLETE)
    DLOG(ERROR) << " Incomplete fb: " << gl::GLEnums::GetStringError(result);
#endif

  // Reshape() must be called to go from using a stencil buffer to not using it.
  DCHECK(use_stencil_ || !stencil_buffer_);
  if (use_stencil_ && !stencil_buffer_) {
    gl->GenRenderbuffers(1, &stencil_buffer_);
    CHECK_NE(stencil_buffer_, 0u);
    gl->BindRenderbuffer(GL_RENDERBUFFER, stencil_buffer_);
    gl->RenderbufferStorage(GL_RENDERBUFFER, GL_STENCIL_INDEX8,
                            reshape_size_.width(), reshape_size_.height());
    gl->BindRenderbuffer(GL_RENDERBUFFER, 0);
    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, stencil_buffer_);
  }
}

gfx::Rect
GpuSurfacelessBrowserCompositorOutputSurface::GetCurrentFramebufferDamage()
    const {
  return buffer_queue_->CurrentBufferDamage();
}

GLenum GpuSurfacelessBrowserCompositorOutputSurface::
    GetFramebufferCopyTextureFormat() {
  return base::strict_cast<GLenum>(
      gl::BufferFormatToGLInternalFormat(buffer_queue_->buffer_format()));
}

void GpuSurfacelessBrowserCompositorOutputSurface::Reshape(
    const gfx::Size& size,
    float device_scale_factor,
    const gfx::ColorSpace& color_space,
    bool has_alpha,
    bool use_stencil) {
  reshape_size_ = size;
  use_stencil_ = use_stencil;
  GpuBrowserCompositorOutputSurface::Reshape(
      size, device_scale_factor, color_space, has_alpha, use_stencil);
  DCHECK(buffer_queue_);

  const bool freed_buffers = buffer_queue_->Reshape(size, color_space);
  if (freed_buffers || (stencil_buffer_ && !use_stencil)) {
    auto* gl = context_provider_->ContextGL();
    gl->BindFramebuffer(GL_FRAMEBUFFER, fbo_);
    if (freed_buffers) {
      gl->FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               texture_target_, 0, 0);
    }
    gl->FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
                                GL_RENDERBUFFER, 0);
    if (stencil_buffer_) {
      gl->DeleteRenderbuffers(1, &stencil_buffer_);
      stencil_buffer_ = 0u;
    }
  }
}

void GpuSurfacelessBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
    std::vector<ui::LatencyInfo> latency_info,
    const gpu::SwapBuffersCompleteParams& params) {
  gpu::SwapBuffersCompleteParams modified_params(params);
  bool force_swap = false;
  if (params.swap_response.result ==
      gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS) {
    // Even through the swap failed, this is a fixable error so we can pretend
    // it succeeded to the rest of the system.
    modified_params.swap_response.result = gfx::SwapResult::SWAP_ACK;
    buffer_queue_->FreeAllSurfaces();
    force_swap = true;
  }
  buffer_queue_->PageFlipComplete();
  GpuBrowserCompositorOutputSurface::OnGpuSwapBuffersCompleted(
      std::move(latency_info), modified_params);
  if (force_swap)
    client_->SetNeedsRedrawRect(gfx::Rect(swap_size_));
}

gpu::SyncToken GpuSurfacelessBrowserCompositorOutputSurface::GenSyncToken() {
  // This should only be called as long as the BufferQueue is alive. We cannot
  // use |buffer_queue_| to detect this because in the dtor, |buffer_queue_|
  // becomes nullptr before BufferQueue's dtor is called, so GenSyncToken()
  // would be called after |buffer_queue_| is nullptr when in fact, the
  // BufferQueue is still alive. Hence, we use |fbo_| to detect that the
  // BufferQueue is still alive.
  DCHECK(fbo_);
  gpu::SyncToken sync_token;
  context_provider_->ContextGL()->GenUnverifiedSyncTokenCHROMIUM(
      sync_token.GetData());
  return sync_token;
}

unsigned GpuSurfacelessBrowserCompositorOutputSurface::UpdateGpuFence() {
  if (!use_gpu_fence_)
    return 0;

  auto* gl = context_provider_->ContextGL();
  if (gpu_fence_id_ > 0)
    gl->DestroyGpuFenceCHROMIUM(gpu_fence_id_);

  gpu_fence_id_ = gl->CreateGpuFenceCHROMIUM();

  return gpu_fence_id_;
}

void GpuSurfacelessBrowserCompositorOutputSurface::SetDrawRectangle(
    const gfx::Rect& damage) {
  GpuBrowserCompositorOutputSurface::SetDrawRectangle(damage);
}

}  // namespace content
