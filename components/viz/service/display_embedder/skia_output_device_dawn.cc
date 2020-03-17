// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_dawn.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "components/viz/common/gpu/dawn_context_provider.h"

#if defined(OS_WIN)
#include <dawn_native/D3D12Backend.h>
#elif defined(OS_LINUX)
#include <dawn_native/VulkanBackend.h>
#endif

namespace viz {

namespace {

// Some Vulkan drivers do not support kRGB_888x_SkColorType. Always use
// kRGBA_8888_SkColorType instead and initialize surface to opaque as necessary.
constexpr SkColorType kSurfaceColorType = kRGBA_8888_SkColorType;
constexpr wgpu::TextureFormat kSwapChainFormat =
    wgpu::TextureFormat::RGBA8Unorm;

constexpr wgpu::TextureUsage kUsage =
    wgpu::TextureUsage::OutputAttachment | wgpu::TextureUsage::CopySrc;

}  // namespace

SkiaOutputDeviceDawn::SkiaOutputDeviceDawn(
    DawnContextProvider* context_provider,
    gfx::AcceleratedWidget widget,
    gfx::SurfaceOrigin origin,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(memory_tracker, did_swap_buffer_complete_callback),
      context_provider_(context_provider),
      widget_(widget) {
  capabilities_.output_surface_origin = origin;
  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.supports_post_sub_buffer = false;

  capabilities_.sk_color_type = kSurfaceColorType;
  capabilities_.gr_backend_format =
      context_provider_->GetGrContext()->defaultBackendFormat(
          kSurfaceColorType, GrRenderable::kYes);
}

SkiaOutputDeviceDawn::~SkiaOutputDeviceDawn() = default;

bool SkiaOutputDeviceDawn::Reshape(const gfx::Size& size,
                                   float device_scale_factor,
                                   const gfx::ColorSpace& color_space,
                                   gfx::BufferFormat format,
                                   gfx::OverlayTransform transform) {
  DCHECK_EQ(transform, gfx::OVERLAY_TRANSFORM_NONE);

  size_ = size;
  sk_color_space_ = color_space.ToSkColorSpace();

  CreateSwapChainImplementation();
  wgpu::SwapChainDescriptor desc;
  desc.implementation = reinterpret_cast<int64_t>(&swap_chain_implementation_);
  // TODO(sgilhuly): Use a wgpu::Surface in this call once the Surface-based
  // SwapChain API is ready.
  swap_chain_ = context_provider_->GetDevice().CreateSwapChain(nullptr, &desc);
  if (!swap_chain_)
    return false;
  swap_chain_.Configure(kSwapChainFormat, kUsage, size_.width(),
                        size_.height());
  return true;
}

void SkiaOutputDeviceDawn::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  StartSwapBuffers(std::move(feedback));
  swap_chain_.Present();
  FinishSwapBuffers(gfx::SwapResult::SWAP_ACK,
                    gfx::Size(size_.width(), size_.height()),
                    std::move(latency_info));
}

SkSurface* SkiaOutputDeviceDawn::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  GrDawnRenderTargetInfo info;
  info.fTextureView = swap_chain_.GetCurrentTextureView();
  info.fFormat = kSwapChainFormat;
  info.fLevelCount = 1;
  GrBackendRenderTarget backend_target(
      size_.width(), size_.height(), /*sampleCnt=*/0, /*stencilBits=*/0, info);
  DCHECK(backend_target.isValid());
  // LegacyFontHost will get LCD text and skia figures out what type to use.
  SkSurfaceProps surface_props(/*flags=*/0,
                               SkSurfaceProps::kLegacyFontHost_InitType);
  sk_surface_ = SkSurface::MakeFromBackendRenderTarget(
      context_provider_->GetGrContext(), backend_target,
      capabilities_.output_surface_origin == gfx::SurfaceOrigin::kTopLeft
          ? kTopLeft_GrSurfaceOrigin
          : kBottomLeft_GrSurfaceOrigin,
      kSurfaceColorType, sk_color_space_, &surface_props);
  return sk_surface_.get();
}

void SkiaOutputDeviceDawn::EndPaint() {
  GrFlushInfo flush_info;
  sk_surface_->flush(SkSurface::BackendSurfaceAccess::kPresent, flush_info);
  sk_surface_.reset();
}

void SkiaOutputDeviceDawn::CreateSwapChainImplementation() {
#if defined(OS_WIN)
  swap_chain_implementation_ = dawn_native::d3d12::CreateNativeSwapChainImpl(
      context_provider_->GetDevice().Get(), widget_);
#else
  NOTREACHED();
  ALLOW_UNUSED_LOCAL(widget_);
#endif
}

}  // namespace viz
