// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_gl.h"

#include <utility>

#include "base/bind_helpers.h"
#include "gpu/command_buffer/common/swap_buffers_complete_params.h"
#include "gpu/command_buffer/service/feature_info.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/core/SkSurfaceProps.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gl/color_space_utils.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_surface.h"
#include "ui/gl/gl_version_info.h"
namespace viz {

SkiaOutputDeviceGL::SkiaOutputDeviceGL(
    scoped_refptr<gl::GLSurface> gl_surface,
    scoped_refptr<gpu::gles2::FeatureInfo> feature_info,
    const DidSwapBufferCompleteCallback& did_swap_buffer_complete_callback)
    : SkiaOutputDevice(false /*need_swap_semaphore */,
                       did_swap_buffer_complete_callback),
      feature_info_(feature_info),
      gl_surface_(gl_surface) {}

void SkiaOutputDeviceGL::Initialize(GrContext* gr_context,
                                    gl::GLContext* gl_context) {
  DCHECK(gr_context);
  DCHECK(gl_context);
  gr_context_ = gr_context;

  gl::CurrentGL* current_gl = gl_context->GetCurrentGL();
  DCHECK(current_gl);

  // Get alpha bits from the default frame buffer.
  glBindFramebufferEXT(GL_FRAMEBUFFER, 0);
  gr_context_->resetContext(kRenderTarget_GrGLBackendState);
  const auto* version = current_gl->Version;
  GLint alpha_bits = 0;
  if (version->is_desktop_core_profile) {
    glGetFramebufferAttachmentParameterivEXT(
        GL_FRAMEBUFFER, GL_BACK_LEFT, GL_FRAMEBUFFER_ATTACHMENT_ALPHA_SIZE,
        &alpha_bits);
  } else {
    glGetIntegerv(GL_ALPHA_BITS, &alpha_bits);
  }
  CHECK_GL_ERROR();
  supports_alpha_ = alpha_bits > 0;

  capabilities_.flipped_output_surface = gl_surface_->FlipsVertically();
  capabilities_.supports_post_sub_buffer = gl_surface_->SupportsPostSubBuffer();
  if (feature_info_->workarounds()
          .disable_post_sub_buffers_for_onscreen_surfaces)
    capabilities_.supports_post_sub_buffer = false;
}

SkiaOutputDeviceGL::~SkiaOutputDeviceGL() {}

scoped_refptr<gl::GLSurface> SkiaOutputDeviceGL::gl_surface() {
  return gl_surface_;
}

void SkiaOutputDeviceGL::Reshape(const gfx::Size& size,
                                 float device_scale_factor,
                                 const gfx::ColorSpace& color_space,
                                 bool has_alpha,
                                 gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  gl::GLSurface::ColorSpace surface_color_space =
      gl::ColorSpaceUtils::GetGLSurfaceColorSpace(color_space);
  if (!gl_surface_->Resize(size, device_scale_factor, surface_color_space,
                           has_alpha)) {
    LOG(FATAL) << "Failed to resize.";
    // TODO(penghuang): Handle the failure.
  }
  SkSurfaceProps surface_props =
      SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);

  GrGLFramebufferInfo framebuffer_info;
  framebuffer_info.fFBOID = gl_surface_->GetBackingFramebufferObject();
  framebuffer_info.fFormat = supports_alpha_ ? GL_RGBA8 : GL_RGB8_OES;
  GrBackendRenderTarget render_target(size.width(), size.height(), 0, 8,
                                      framebuffer_info);
  auto origin = gl_surface_->FlipsVertically() ? kTopLeft_GrSurfaceOrigin
                                               : kBottomLeft_GrSurfaceOrigin;
  auto color_type =
      supports_alpha_ ? kRGBA_8888_SkColorType : kRGB_888x_SkColorType;
  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      gr_context_, render_target, origin, color_type,
      color_space.ToSkColorSpace(), &surface_props);
  DCHECK(sk_surface_);
}

void SkiaOutputDeviceGL::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  if (gl_surface_->SupportsAsyncSwap()) {
    auto callback = base::BindOnce(&SkiaOutputDeviceGL::DoFinishSwapBuffers,
                                   weak_ptr_factory_.GetWeakPtr(), surface_size,
                                   std::move(latency_info));
    gl_surface_->SwapBuffersAsync(std::move(callback), std::move(feedback));
  } else {
    FinishSwapBuffers(gl_surface_->SwapBuffers(std::move(feedback)),
                      surface_size, std::move(latency_info));
  }
}

void SkiaOutputDeviceGL::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers({});

  gfx::Size surface_size =
      gfx::Size(sk_surface_->width(), sk_surface_->height());

  if (gl_surface_->SupportsAsyncSwap()) {
    auto callback = base::BindOnce(&SkiaOutputDeviceGL::DoFinishSwapBuffers,
                                   weak_ptr_factory_.GetWeakPtr(), surface_size,
                                   std::move(latency_info));
    gl_surface_->PostSubBufferAsync(rect.x(), rect.y(), rect.width(),
                                    rect.height(), std::move(callback),
                                    std::move(feedback));

  } else {
    FinishSwapBuffers(
        gl_surface_->PostSubBuffer(rect.x(), rect.y(), rect.width(),
                                   rect.height(), std::move(feedback)),
        surface_size, std::move(latency_info));
  }
}

void SkiaOutputDeviceGL::DoFinishSwapBuffers(
    const gfx::Size& size,
    std::vector<ui::LatencyInfo> latency_info,
    gfx::SwapResult result,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  DCHECK(!gpu_fence);
  FinishSwapBuffers(result, size, latency_info);
}

void SkiaOutputDeviceGL::SetDrawRectangle(const gfx::Rect& draw_rectangle) {
  gl_surface_->SetDrawRectangle(draw_rectangle);
}

void SkiaOutputDeviceGL::EnsureBackbuffer() {
  gl_surface_->SetBackbufferAllocation(true);
}

void SkiaOutputDeviceGL::DiscardBackbuffer() {
  gl_surface_->SetBackbufferAllocation(false);
}

SkSurface* SkiaOutputDeviceGL::BeginPaint() {
  DCHECK(sk_surface_);
  return sk_surface_.get();
}

void SkiaOutputDeviceGL::EndPaint(const GrBackendSemaphore& semaphore) {}

#if defined(OS_WIN)
void SkiaOutputDeviceGL::DidCreateAcceleratedSurfaceChildWindow(
    gpu::SurfaceHandle parent_window,
    gpu::SurfaceHandle child_window) {
  NOTREACHED();
}
#endif

const gpu::gles2::FeatureInfo* SkiaOutputDeviceGL::GetFeatureInfo() const {
  return feature_info_.get();
}

const gpu::GpuPreferences& SkiaOutputDeviceGL::GetGpuPreferences() const {
  return gpu_preferences_;
}

void SkiaOutputDeviceGL::DidSwapBuffersComplete(
    gpu::SwapBuffersCompleteParams params) {
  // TODO(kylechar): Check if this is necessary.
}

void SkiaOutputDeviceGL::BufferPresented(
    const gfx::PresentationFeedback& feedback) {
  // TODO(kylechar): Check if this is necessary.
}

GpuVSyncCallback SkiaOutputDeviceGL::GetGpuVSyncCallback() {
  // TODO(sunnyps): Implement GpuVSync with SkiaRenderer.
  NOTIMPLEMENTED();
  return base::DoNothing::Repeatedly<base::TimeTicks, base::TimeDelta>();
}

}  // namespace viz
