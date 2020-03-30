// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/win32/vulkan_implementation_win32.h"

#include <Windows.h>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace gpu {

VulkanImplementationWin32::VulkanImplementationWin32(bool use_swiftshader)
    : VulkanImplementation(use_swiftshader) {}

VulkanImplementationWin32::~VulkanImplementationWin32() = default;

bool VulkanImplementationWin32::InitializeVulkanInstance(bool using_surface) {
  DCHECK(using_surface);
  std::vector<const char*> required_extensions = {
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
  };

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  base::FilePath path(use_swiftshader() ? L"vk_swiftshader.dll"
                                        : L"vulkan-1.dll");

  base::NativeLibraryLoadError native_library_load_error;
  vulkan_function_pointers->vulkan_loader_library =
      base::LoadNativeLibrary(path, &native_library_load_error);
  if (!vulkan_function_pointers->vulkan_loader_library)
    return false;

  if (!vulkan_instance_.Initialize(required_extensions, {}))
    return false;
  return true;
}

VulkanInstance* VulkanImplementationWin32::GetVulkanInstance() {
  return &vulkan_instance_;
}

std::unique_ptr<VulkanSurface> VulkanImplementationWin32::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  VkSurfaceKHR surface;
  VkWin32SurfaceCreateInfoKHR surface_create_info = {};
  surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  surface_create_info.hinstance =
      reinterpret_cast<HINSTANCE>(GetWindowLongPtr(window, GWLP_HINSTANCE));
  surface_create_info.hwnd = window;
  VkResult result = vkCreateWin32SurfaceKHR(
      vulkan_instance_.vk_instance(), &surface_create_info, nullptr, &surface);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreatWin32SurfaceKHR() failed: " << result;
    return nullptr;
  }

  return std::make_unique<VulkanSurface>(vulkan_instance_.vk_instance(),
                                         surface,
                                         /* use_protected_memory */ false);
}

bool VulkanImplementationWin32::GetPhysicalDevicePresentationSupport(
    VkPhysicalDevice device,
    const std::vector<VkQueueFamilyProperties>& queue_family_properties,
    uint32_t queue_family_index) {
  return vkGetPhysicalDeviceWin32PresentationSupportKHR(device,
                                                        queue_family_index);
}

std::vector<const char*>
VulkanImplementationWin32::GetRequiredDeviceExtensions() {
  return {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
}

std::vector<const char*>
VulkanImplementationWin32::GetOptionalDeviceExtensions() {
  return {};
}

VkFence VulkanImplementationWin32::CreateVkFenceForGpuFence(
    VkDevice vk_device) {
  NOTREACHED();
  return VK_NULL_HANDLE;
}

std::unique_ptr<gfx::GpuFence>
VulkanImplementationWin32::ExportVkFenceToGpuFence(VkDevice vk_device,
                                                   VkFence vk_fence) {
  NOTREACHED();
  return nullptr;
}

VkSemaphore VulkanImplementationWin32::CreateExternalSemaphore(
    VkDevice vk_device) {
  NOTIMPLEMENTED();
  return VK_NULL_HANDLE;
}

VkSemaphore VulkanImplementationWin32::ImportSemaphoreHandle(
    VkDevice vk_device,
    SemaphoreHandle handle) {
  NOTIMPLEMENTED();
  return VK_NULL_HANDLE;
}

SemaphoreHandle VulkanImplementationWin32::GetSemaphoreHandle(
    VkDevice vk_device,
    VkSemaphore vk_semaphore) {
  return SemaphoreHandle();
}

VkExternalMemoryHandleTypeFlagBits
VulkanImplementationWin32::GetExternalImageHandleType() {
  return VK_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_TEXTURE_BIT;
}

bool VulkanImplementationWin32::CanImportGpuMemoryBuffer(
    gfx::GpuMemoryBufferType memory_buffer_type) {
  return false;
}

std::unique_ptr<VulkanImage>
VulkanImplementationWin32::CreateImageFromGpuMemoryHandle(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    gfx::Size size,
    VkFormat vk_formae) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace gpu
