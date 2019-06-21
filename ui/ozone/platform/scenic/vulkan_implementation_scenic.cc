// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/vulkan_implementation_scenic.h"

#include <lib/ui/scenic/cpp/commands.h>
#include <lib/ui/scenic/cpp/session.h>
#include <lib/zx/channel.h>
#include <vulkan/vulkan.h>
#include <memory>

#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/macros.h"
#include "base/native_library.h"
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_util.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/ozone/platform/scenic/scenic_surface.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"
#include "ui/ozone/platform/scenic/scenic_window.h"
#include "ui/ozone/platform/scenic/scenic_window_manager.h"
#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"

namespace ui {

VulkanImplementationScenic::VulkanImplementationScenic(
    ScenicSurfaceFactory* scenic_surface_factory,
    SysmemBufferManager* sysmem_buffer_manager)
    : scenic_surface_factory_(scenic_surface_factory),
      sysmem_buffer_manager_(sysmem_buffer_manager) {}

VulkanImplementationScenic::~VulkanImplementationScenic() = default;

bool VulkanImplementationScenic::InitializeVulkanInstance(bool using_surface) {
  DCHECK(using_surface);
  base::NativeLibraryLoadError error;
  base::NativeLibrary handle =
      base::LoadNativeLibrary(base::FilePath("libvulkan.so"), &error);
  if (!handle) {
    LOG(ERROR) << "Failed to load vulkan: " << error.ToString();
    return false;
  }

  gpu::VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  vulkan_function_pointers->vulkan_loader_library_ = handle;
  std::vector<const char*> required_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME,
  };
  std::vector<const char*> required_layers = {
      "VK_LAYER_FUCHSIA_imagepipe_swapchain",
  };
  if (!vulkan_instance_.Initialize(required_extensions, required_layers))
    return false;

  vkCreateImagePipeSurfaceFUCHSIA_ =
      reinterpret_cast<PFN_vkCreateImagePipeSurfaceFUCHSIA>(
          vkGetInstanceProcAddr(vulkan_instance_.vk_instance(),
                                "vkCreateImagePipeSurfaceFUCHSIA"));
  if (!vkCreateImagePipeSurfaceFUCHSIA_)
    return false;

  return true;
}

gpu::VulkanInstance* VulkanImplementationScenic::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<gpu::VulkanSurface>
VulkanImplementationScenic::CreateViewSurface(gfx::AcceleratedWidget window) {
  fuchsia::images::ImagePipePtr image_pipe;
  ScenicSurface* scenic_surface = scenic_surface_factory_->GetSurface(window);
  scenic_surface->SetTextureToNewImagePipe(image_pipe.NewRequest());

  VkSurfaceKHR surface;
  VkImagePipeSurfaceCreateInfoFUCHSIA surface_create_info = {};
  surface_create_info.sType =
      VK_STRUCTURE_TYPE_IMAGEPIPE_SURFACE_CREATE_INFO_FUCHSIA;
  surface_create_info.flags = 0;
  surface_create_info.imagePipeHandle =
      image_pipe.Unbind().TakeChannel().release();

  VkResult result = vkCreateImagePipeSurfaceFUCHSIA_(
      vulkan_instance_.vk_instance(), &surface_create_info, nullptr, &surface);
  if (result != VK_SUCCESS) {
    // This shouldn't fail, and we don't know whether imagePipeHandle was closed
    // if it does.
    LOG(FATAL) << "vkCreateImagePipeSurfaceFUCHSIA failed: " << result;
  }

  return std::make_unique<gpu::VulkanSurface>(vulkan_instance_.vk_instance(),
                                              surface);
}

bool VulkanImplementationScenic::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice physical_device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  // TODO(spang): vkGetPhysicalDeviceMagmaPresentationSupportKHR returns false
  // here. Use it once it is fixed.
  NOTIMPLEMENTED();
  return true;
}

std::vector<const char*>
VulkanImplementationScenic::GetRequiredDeviceExtensions() {
  return {
      VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME,
      VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
      VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME,
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
}

VkFence VulkanImplementationScenic::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTIMPLEMENTED();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationScenic::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                    VkFence vk_fence) {
  NOTIMPLEMENTED();
  return nullptr;
}

VkSemaphore VulkanImplementationScenic::CreateExternalSemaphore(
    VkDevice vk_device) {
  return gpu::CreateExternalVkSemaphore(
      vk_device,
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA);
}

VkSemaphore VulkanImplementationScenic::ImportSemaphoreHandle(
    VkDevice vk_device,
    gpu::SemaphoreHandle handle) {
  if (!handle.is_valid())
    return VK_NULL_HANDLE;

  if (handle.vk_handle_type() !=
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA) {
    return VK_NULL_HANDLE;
  }

  VkSemaphore semaphore = VK_NULL_HANDLE;
  VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
  VkResult result = vkCreateSemaphore(vk_device, &info, nullptr, &semaphore);
  if (result != VK_SUCCESS)
    return VK_NULL_HANDLE;

  zx::event event = handle.TakeHandle();
  VkImportSemaphoreZirconHandleInfoFUCHSIA import = {
      VK_STRUCTURE_TYPE_TEMP_IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA};
  import.semaphore = semaphore;
  import.handleType =
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA;
  import.handle = event.get();

  result = vkImportSemaphoreZirconHandleFUCHSIA(vk_device, &import);
  if (result != VK_SUCCESS) {
    vkDestroySemaphore(vk_device, semaphore, nullptr);
    return VK_NULL_HANDLE;
  }

  // Vulkan took ownership of the handle.
  ignore_result(event.release());

  return semaphore;
}

gpu::SemaphoreHandle VulkanImplementationScenic::GetSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore) {
  // Create VkSemaphoreGetFdInfoKHR structure.
  VkSemaphoreGetZirconHandleInfoFUCHSIA info = {
      VK_STRUCTURE_TYPE_TEMP_SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA};
  info.semaphore = vk_semaphore;
  info.handleType =
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA;

  zx_handle_t handle;
  VkResult result =
      vkGetSemaphoreZirconHandleFUCHSIA(vk_device, &info, &handle);
  if (result != VK_SUCCESS) {
    LOG(ERROR) << "vkGetSemaphoreFuchsiaHandleKHR failed : " << result;
    return gpu::SemaphoreHandle();
  }

  return gpu::SemaphoreHandle(
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA,
      zx::event(handle));
}

VkExternalMemoryHandleTypeFlagBits
VulkanImplementationScenic::GetExternalImageHandleType() {
  return VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA;
}

bool VulkanImplementationScenic::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return memory_buffer_type == gfx::NATIVE_PIXMAP;
}

bool VulkanImplementationScenic::CreateImageFromGpuMemoryHandle(
    VkDevice vk_device,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkImage* vk_image,
    VkImageCreateInfo* vk_image_info,
    VkDeviceMemory* vk_device_memory,
    VkDeviceSize* mem_allocation_size) {
  if (gmb_handle.type != gfx::NATIVE_PIXMAP)
    return false;

  if (!gmb_handle.native_pixmap_handle.buffer_collection_id) {
    DLOG(ERROR) << "NativePixmapHandle.buffer_collection_id is not set.";
    return false;
  }

  auto collection = sysmem_buffer_manager_->GetCollectionById(
      gmb_handle.native_pixmap_handle.buffer_collection_id.value());
  if (!collection) {
    DLOG(ERROR) << "Tried to use an unknown buffer collection ID";
    return false;
  }

  if (gmb_handle.native_pixmap_handle.buffer_index >=
          collection->num_buffers() ||
      size != collection->size()) {
    DLOG(ERROR)
        << "Can't import GpuMemoryBuffer to an image with a different size.";
    return false;
  }

  return collection->CreateVkImage(gmb_handle.native_pixmap_handle.buffer_index,
                                   vk_device, vk_image, vk_image_info,
                                   vk_device_memory, mem_allocation_size);
}

}  // namespace ui
