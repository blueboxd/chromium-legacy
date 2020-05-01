// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/skia_output_device_vulkan.h"

#include <utility>

#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/ipc/common/gpu_surface_lookup.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/vk/GrVkTypes.h"

namespace viz {

// static
std::unique_ptr<SkiaOutputDeviceVulkan> SkiaOutputDeviceVulkan::Create(
    VulkanContextProvider* context_provider,
    gpu::SurfaceHandle surface_handle,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback) {
  auto output_device = std::make_unique<SkiaOutputDeviceVulkan>(
      util::PassKey<SkiaOutputDeviceVulkan>(), context_provider, surface_handle,
      memory_tracker, did_swap_buffer_complete_callback);
  if (!output_device->Initialize())
    return nullptr;
  return output_device;
}

SkiaOutputDeviceVulkan::SkiaOutputDeviceVulkan(
    util::PassKey<SkiaOutputDeviceVulkan>,
    VulkanContextProvider* context_provider,
    gpu::SurfaceHandle surface_handle,
    gpu::MemoryTracker* memory_tracker,
    DidSwapBufferCompleteCallback did_swap_buffer_complete_callback)
    : SkiaOutputDevice(memory_tracker, did_swap_buffer_complete_callback),
      context_provider_(context_provider),
      surface_handle_(surface_handle) {}

SkiaOutputDeviceVulkan::~SkiaOutputDeviceVulkan() {
  DCHECK(!scoped_write_);
  for (auto it = sk_surface_size_pairs_.begin();
       it != sk_surface_size_pairs_.end(); ++it) {
    memory_type_tracker_->TrackMemFree(it->bytes_allocated);
  }
  sk_surface_size_pairs_.clear();

  if (!vulkan_surface_)
    return;

    vkQueueWaitIdle(context_provider_->GetDeviceQueue()->GetVulkanQueue());
    vulkan_surface_->Destroy();
}

#if defined(OS_WIN)
gpu::SurfaceHandle SkiaOutputDeviceVulkan::GetChildSurfaceHandle() {
  if (vulkan_surface_->accelerated_widget() != surface_handle_)
    return vulkan_surface_->accelerated_widget();
  return gpu::kNullSurfaceHandle;
}
#endif

bool SkiaOutputDeviceVulkan::Reshape(const gfx::Size& size,
                                     float device_scale_factor,
                                     const gfx::ColorSpace& color_space,
                                     gfx::BufferFormat format,
                                     gfx::OverlayTransform transform) {
  DCHECK(!scoped_write_);

  if (!vulkan_surface_)
    return false;

  return RecreateSwapChain(size, color_space.ToSkColorSpace(), transform);
}

void SkiaOutputDeviceVulkan::SwapBuffers(
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  PostSubBuffer(gfx::Rect(vulkan_surface_->image_size()), std::move(feedback),
                std::move(latency_info));
}

void SkiaOutputDeviceVulkan::PostSubBuffer(
    const gfx::Rect& rect,
    BufferPresentedCallback feedback,
    std::vector<ui::LatencyInfo> latency_info) {
  // Reshape should have been called first.
  DCHECK(vulkan_surface_);
  DCHECK(!scoped_write_);
#if DCHECK_IS_ON()
  DCHECK_EQ(!rect.IsEmpty(), image_modified_);
  image_modified_ = false;
#endif

  StartSwapBuffers(std::move(feedback));
  auto image_size = vulkan_surface_->image_size();
  gfx::SwapResult result = gfx::SwapResult::SWAP_ACK;
  // If the swapchain is new created, but rect doesn't cover the whole buffer,
  // we will still present it even it causes a artifact in this frame and
  // recovered when the next frame is presented. We do that because the old
  // swapchain's present thread is blocked on waiting a reply from xserver, and
  // presenting a new image with the new create swapchain will somehow makes
  // xserver send a reply to us, and then unblock the old swapchain's present
  // thread. So the old swapchain can be destroyed properly.
  if (!rect.IsEmpty())
    result = vulkan_surface_->PostSubBuffer(rect);
  if (is_new_swapchain_) {
    is_new_swapchain_ = false;
    result = gfx::SwapResult::SWAP_NAK_RECREATE_BUFFERS;
  }
  FinishSwapBuffers(result, image_size, std::move(latency_info));
}

SkSurface* SkiaOutputDeviceVulkan::BeginPaint(
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK(vulkan_surface_);
  DCHECK(!scoped_write_);

  scoped_write_.emplace(vulkan_surface_->swap_chain());
  if (!scoped_write_->success()) {
    scoped_write_.reset();
    if (vulkan_surface_->swap_chain()->state() != VK_ERROR_SURFACE_LOST_KHR)
      return nullptr;
    // If vulkan surface is lost, we will try to recreate swap chain.
    if (!RecreateSwapChain(vulkan_surface_->image_size(), color_space_,
                           vulkan_surface_->transform())) {
      LOG(DFATAL) << "Failed to recreate vulkan swap chain.";
      return nullptr;
    }
    scoped_write_.emplace(vulkan_surface_->swap_chain());
    if (!scoped_write_->success()) {
      scoped_write_.reset();
      return nullptr;
    }
  }

  auto& sk_surface =
      sk_surface_size_pairs_[scoped_write_->image_index()].sk_surface;

  if (!sk_surface) {
    SkSurfaceProps surface_props =
        SkSurfaceProps(0, SkSurfaceProps::kLegacyFontHost_InitType);
    const auto surface_format = vulkan_surface_->surface_format().format;
    DCHECK(surface_format == VK_FORMAT_B8G8R8A8_UNORM ||
           surface_format == VK_FORMAT_R8G8B8A8_UNORM);
    GrVkImageInfo vk_image_info(
        scoped_write_->image(), GrVkAlloc(), VK_IMAGE_TILING_OPTIMAL,
        scoped_write_->image_layout(), surface_format, 1 /* level_count */,
        VK_QUEUE_FAMILY_IGNORED,
        vulkan_surface_->swap_chain()->use_protected_memory()
            ? GrProtected::kYes
            : GrProtected::kNo);
    const auto& vk_image_size = vulkan_surface_->image_size();
    GrBackendRenderTarget render_target(vk_image_size.width(),
                                        vk_image_size.height(),
                                        0 /* sample_cnt */, vk_image_info);

    // Estimate size of GPU memory needed for the GrBackendRenderTarget.
    VkMemoryRequirements requirements;
    vkGetImageMemoryRequirements(
        context_provider_->GetDeviceQueue()->GetVulkanDevice(),
        vk_image_info.fImage, &requirements);
    sk_surface_size_pairs_[scoped_write_->image_index()].bytes_allocated =
        requirements.size;
    memory_type_tracker_->TrackMemAlloc(requirements.size);

    auto sk_color_type = surface_format == VK_FORMAT_B8G8R8A8_UNORM
                             ? kBGRA_8888_SkColorType
                             : kRGBA_8888_SkColorType;
    sk_surface = SkSurface::MakeFromBackendRenderTarget(
        context_provider_->GetGrContext(), render_target,
        kTopLeft_GrSurfaceOrigin, sk_color_type, color_space_, &surface_props);
    DCHECK(sk_surface);
  } else {
    auto backend = sk_surface->getBackendRenderTarget(
        SkSurface::kFlushRead_BackendHandleAccess);
    backend.setVkImageLayout(scoped_write_->image_layout());
  }
  VkSemaphore vk_semaphore = scoped_write_->TakeBeginSemaphore();
  if (vk_semaphore != VK_NULL_HANDLE) {
    GrBackendSemaphore semaphore;
    semaphore.initVulkan(vk_semaphore);
    auto result = sk_surface->wait(1, &semaphore);
    DCHECK(result);
  }

  GrBackendSemaphore end_semaphore;
  end_semaphore.initVulkan(scoped_write_->GetEndSemaphore());
  end_semaphores->push_back(std::move(end_semaphore));

  return sk_surface.get();
}

void SkiaOutputDeviceVulkan::EndPaint() {
  DCHECK(scoped_write_);

  auto& sk_surface =
      sk_surface_size_pairs_[scoped_write_->image_index()].sk_surface;
  auto backend = sk_surface->getBackendRenderTarget(
      SkSurface::kFlushRead_BackendHandleAccess);
  GrVkImageInfo vk_image_info;
  if (!backend.getVkImageInfo(&vk_image_info))
    NOTREACHED() << "Failed to get the image info.";
  scoped_write_->set_image_layout(vk_image_info.fImageLayout);
  scoped_write_.reset();
#if DCHECK_IS_ON()
  image_modified_ = true;
#endif
}

bool SkiaOutputDeviceVulkan::Initialize() {
  gfx::AcceleratedWidget accelerated_widget = gfx::kNullAcceleratedWidget;
#if defined(OS_ANDROID)
  bool can_be_used_with_surface_control = false;
  accelerated_widget =
      gpu::GpuSurfaceLookup::GetInstance()->AcquireNativeWidget(
          surface_handle_, &can_be_used_with_surface_control);
#else
  accelerated_widget = surface_handle_;
#endif
  auto vulkan_surface =
      context_provider_->GetVulkanImplementation()->CreateViewSurface(
          accelerated_widget);
  if (!vulkan_surface) {
    LOG(ERROR) << "Failed to create vulkan surface.";
    return false;
  }
  if (!vulkan_surface->Initialize(context_provider_->GetDeviceQueue(),
                                  gpu::VulkanSurface::FORMAT_RGBA_32)) {
    LOG(ERROR) << "Failed to initialize vulkan surface.";
    return false;
  }
  vulkan_surface_ = std::move(vulkan_surface);

  capabilities_.uses_default_gl_framebuffer = false;
  capabilities_.max_frames_pending = vulkan_surface_->image_count() - 1;
  // Vulkan FIFO swap chain should return vk images in presenting order, so set
  // preserve_buffer_content & supports_post_sub_buffer to true to let
  // SkiaOutputBufferImpl to manager damages.
  capabilities_.preserve_buffer_content = true;
  capabilities_.output_surface_origin = gfx::SurfaceOrigin::kTopLeft;
  capabilities_.supports_post_sub_buffer = true;
  capabilities_.supports_pre_transform = true;

  const auto surface_format = vulkan_surface_->surface_format().format;
  DCHECK(surface_format == VK_FORMAT_B8G8R8A8_UNORM ||
         surface_format == VK_FORMAT_R8G8B8A8_UNORM);
  capabilities_.sk_color_type = surface_format == VK_FORMAT_R8G8B8A8_UNORM
                                    ? kRGBA_8888_SkColorType
                                    : kBGRA_8888_SkColorType;
  capabilities_.gr_backend_format = GrBackendFormat::MakeVk(surface_format);
  return true;
}

bool SkiaOutputDeviceVulkan::RecreateSwapChain(
    const gfx::Size& size,
    sk_sp<SkColorSpace> color_space,
    gfx::OverlayTransform transform) {
  auto generation = vulkan_surface_->swap_chain_generation();

  // Call vulkan_surface_->Reshape() will recreate vulkan swapchain if it is
  // necessary.
  if (!vulkan_surface_->Reshape(size, transform))
    return false;

  if (vulkan_surface_->swap_chain_generation() != generation ||
      !SkColorSpace::Equals(color_space.get(), color_space_.get())) {
    // swapchain is changed, we need recreate all cached sk surfaces.
    for (const auto& sk_surface_size_pair : sk_surface_size_pairs_) {
      memory_type_tracker_->TrackMemFree(sk_surface_size_pair.bytes_allocated);
    }
    sk_surface_size_pairs_.clear();
    sk_surface_size_pairs_.resize(vulkan_surface_->swap_chain()->num_images());
    color_space_ = std::move(color_space);
    is_new_swapchain_ = true;
  }

  return true;
}

SkiaOutputDeviceVulkan::SkSurfaceSizePair::SkSurfaceSizePair() = default;
SkiaOutputDeviceVulkan::SkSurfaceSizePair::SkSurfaceSizePair(
    const SkSurfaceSizePair& other) = default;
SkiaOutputDeviceVulkan::SkSurfaceSizePair::~SkSurfaceSizePair() = default;

}  // namespace viz
