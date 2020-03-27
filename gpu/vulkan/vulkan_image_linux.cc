// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_image.h"

#include "gpu/vulkan/vulkan_device_queue.h"

namespace gpu {

bool VulkanImage::InitializeFromGpuMemoryBufferHandle(
    VulkanDeviceQueue* device_queue,
    gfx::GpuMemoryBufferHandle gmb_handle,
    const gfx::Size& size,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageCreateFlags flags,
    VkImageTiling image_tiling) {
  if (gmb_handle.type != gfx::GpuMemoryBufferType::NATIVE_PIXMAP) {
    DLOG(ERROR) << "GpuMemoryBuffer is not supported. type:" << gmb_handle.type;
    return false;
  }

  auto& native_pixmap_handle = gmb_handle.native_pixmap_handle;
  DCHECK_EQ(native_pixmap_handle.planes.size(), 1u);

  auto& scoped_fd = native_pixmap_handle.planes[0].fd;
  if (!scoped_fd.is_valid()) {
    DLOG(ERROR) << "GpuMemoryBufferHandle doesn't have a valid fd.";
    return false;
  }

  bool using_modifier =
      native_pixmap_handle.modifier != gfx::NativePixmapHandle::kNoModifier &&
      gfx::HasExtension(device_queue->enabled_extensions(),
                        VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME);

  // If the driver doesn't support modifier or the native_pixmap_handle doesn't
  // have modifier, VK_IMAGE_TILING_OPTIMAL will be used.
  DCHECK_EQ(image_tiling, VK_IMAGE_TILING_OPTIMAL);
  if (using_modifier)
    image_tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;

  VkExternalMemoryImageCreateInfoKHR external_image_create_info = {
      .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO_KHR,
      .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
  };
  VkImageDrmFormatModifierListCreateInfoEXT modifier_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_LIST_CREATE_INFO_EXT,
      .drmFormatModifierCount = 1,
      .pDrmFormatModifiers = &native_pixmap_handle.modifier,
  };
  if (using_modifier)
    external_image_create_info.pNext = &modifier_info;

  VkImportMemoryFdInfoKHR import_memory_fd_info = {
      .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
      .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
      .fd = scoped_fd.get(),
  };

  VkMemoryRequirements* requirements = nullptr;
  bool result = Initialize(device_queue, size, format, usage, flags,
                           image_tiling, &external_image_create_info,
                           &import_memory_fd_info, requirements);
  // If Initialize successfully, the fd in scoped_fd should be owned by vulkan.
  if (result)
    ignore_result(scoped_fd.release());

  return result;
}

}  // namespace gpu