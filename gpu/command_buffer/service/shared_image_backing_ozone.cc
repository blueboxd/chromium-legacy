// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_backing_ozone.h"

#include <dawn/webgpu.h>
#include <vulkan/vulkan.h>

#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_manager.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/buffer_types.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/buildflags.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"

#if BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/shared_image_representation_dawn_ozone.h"
#endif  // BUILDFLAG(USE_DAWN)

namespace gpu {
namespace {

size_t GetPixmapSizeInBytes(const gfx::NativePixmap& pixmap) {
  return gfx::BufferSizeForBufferFormat(pixmap.GetBufferSize(),
                                        pixmap.GetBufferFormat());
}

gfx::BufferUsage GetBufferUsage(uint32_t usage) {
  if (usage & SHARED_IMAGE_USAGE_WEBGPU) {
    // Just use SCANOUT for WebGPU since the memory doesn't need to be linear.
    return gfx::BufferUsage::SCANOUT;
  } else if (usage & SHARED_IMAGE_USAGE_SCANOUT) {
    return gfx::BufferUsage::SCANOUT;
  } else {
    NOTREACHED() << "Unsupported usage flags.";
    return gfx::BufferUsage::SCANOUT;
  }
}

}  // namespace

std::unique_ptr<SharedImageBackingOzone> SharedImageBackingOzone::Create(
    scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs,
    SharedContextState* context_state,
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage) {
  gfx::BufferFormat buffer_format = viz::BufferFormat(format);
  gfx::BufferUsage buffer_usage = GetBufferUsage(usage);
  VkDevice vk_device = VK_NULL_HANDLE;
  DCHECK(context_state);
  if (context_state->vk_context_provider()) {
    vk_device = context_state->vk_context_provider()
                    ->GetDeviceQueue()
                    ->GetVulkanDevice();
  }
  ui::SurfaceFactoryOzone* surface_factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();
  scoped_refptr<gfx::NativePixmap> pixmap = surface_factory->CreateNativePixmap(
      gfx::kNullAcceleratedWidget, vk_device, size, buffer_format,
      buffer_usage);
  if (!pixmap) {
    return nullptr;
  }

  return base::WrapUnique(new SharedImageBackingOzone(
      mailbox, format, size, color_space, usage, context_state,
      std::move(pixmap), std::move(dawn_procs)));
}

SharedImageBackingOzone::~SharedImageBackingOzone() = default;

gfx::Rect SharedImageBackingOzone::ClearedRect() const {
  NOTIMPLEMENTED_LOG_ONCE();
  return gfx::Rect();
}

void SharedImageBackingOzone::SetClearedRect(const gfx::Rect& cleared_rect) {
  NOTIMPLEMENTED_LOG_ONCE();
}

void SharedImageBackingOzone::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  NOTIMPLEMENTED_LOG_ONCE();
  return;
}

bool SharedImageBackingOzone::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  NOTREACHED();
  return false;
}

std::unique_ptr<SharedImageRepresentationDawn>
SharedImageBackingOzone::ProduceDawn(SharedImageManager* manager,
                                     MemoryTypeTracker* tracker,
                                     WGPUDevice device) {
#if BUILDFLAG(USE_DAWN)
  WGPUTextureFormat webgpu_format = viz::ToWGPUFormat(format());
  if (webgpu_format == WGPUTextureFormat_Undefined) {
    return nullptr;
  }
  return std::make_unique<SharedImageRepresentationDawnOzone>(
      manager, this, tracker, device, webgpu_format, pixmap_, dawn_procs_);
#else  // !BUILDFLAG(USE_DAWN)
  return nullptr;
#endif
}

std::unique_ptr<SharedImageRepresentationGLTexture>
SharedImageBackingOzone::ProduceGLTexture(SharedImageManager* manager,
                                          MemoryTypeTracker* tracker) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
SharedImageBackingOzone::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationSkia>
SharedImageBackingOzone::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<SharedImageRepresentationOverlay>
SharedImageBackingOzone::ProduceOverlay(SharedImageManager* manager,
                                        MemoryTypeTracker* tracker) {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

SharedImageBackingOzone::SharedImageBackingOzone(
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    SharedContextState* context_state,
    scoped_refptr<gfx::NativePixmap> pixmap,
    scoped_refptr<base::RefCountedData<DawnProcTable>> dawn_procs)
    : SharedImageBacking(mailbox,
                         format,
                         size,
                         color_space,
                         usage,
                         GetPixmapSizeInBytes(*pixmap),
                         false),
      pixmap_(std::move(pixmap)),
      dawn_procs_(std::move(dawn_procs)) {}

}  // namespace gpu
