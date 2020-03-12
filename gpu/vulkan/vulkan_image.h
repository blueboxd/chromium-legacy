// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_IMAGE_H_
#define GPU_VULKAN_VULKAN_IMAGE_H_

#include <vulkan/vulkan.h>

#include "base/optional.h"
#include "base/util/type_safety/pass_key.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/geometry/size.h"

namespace gpu {

class VulkanDeviceQueue;

class VULKAN_EXPORT VulkanImage {
 public:
  explicit VulkanImage(util::PassKey<VulkanImage> pass_key);
  ~VulkanImage();

  VulkanImage(VulkanImage&) = delete;
  VulkanImage& operator=(VulkanImage&) = delete;

  static std::unique_ptr<VulkanImage> Create(
      VulkanDeviceQueue* device_queue,
      const gfx::Size& size,
      VkFormat vk_format,
      VkImageUsageFlags vk_usage,
      VkImageCreateFlags vk_flags = 0,
      void* vk_image_create_info_next = nullptr,
      void* vk_memory_allocation_info_next = nullptr);

  static std::unique_ptr<VulkanImage> Create(
      VulkanDeviceQueue* device_queue,
      VkImage vk_image,
      VkDeviceMemory vk_device_memory,
      const gfx::Size& size,
      VkFormat vk_format,
      VkImageTiling vk_image_tiling,
      VkDeviceSize vk_device_size,
      uint32_t memory_type_index,
      base::Optional<VulkanYCbCrInfo>& ycbcr_info);

  void Destroy();

  const gfx::Size& size() const { return size_; }
  VkFormat format() const { return format_; }
  VkDeviceSize device_size() const { return device_size_; }
  uint32_t memory_type_index() const { return memory_type_index_; }
  VkImageTiling image_tiling() const { return image_tiling_; }
  const base::Optional<VulkanYCbCrInfo>& ycbcr_info() const {
    return ycbcr_info_;
  }
  VkImage image() const { return image_; }
  VkDeviceMemory device_memory() const { return device_memory_; }

 private:
  bool Initialize(VulkanDeviceQueue* device_queue,
                  const gfx::Size& size,
                  VkFormat vk_format,
                  VkImageUsageFlags vk_usage,
                  VkImageCreateFlags vk_flags,
                  void* vk_image_create_info_next,
                  void* vk_memory_allocation_info_next);

  VulkanDeviceQueue* device_queue_ = nullptr;
  gfx::Size size_;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkDeviceSize device_size_ = 0;
  uint32_t memory_type_index_ = 0;
  VkImageTiling image_tiling_ = VK_IMAGE_TILING_OPTIMAL;
  base::Optional<VulkanYCbCrInfo> ycbcr_info_;
  VkImage image_ = VK_NULL_HANDLE;
  VkDeviceMemory device_memory_ = VK_NULL_HANDLE;
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_IMAGE_H_
