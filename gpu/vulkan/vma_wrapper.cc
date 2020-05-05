// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vma_wrapper.h"

#include <vk_mem_alloc.h>

#include "build/build_config.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {
namespace vma {

VkResult CreateAllocator(VkPhysicalDevice physical_device,
                         VkDevice device,
                         VkInstance instance,
                         VmaAllocator* pAllocator) {
  VmaVulkanFunctions functions = {
      vkGetPhysicalDeviceProperties.get(),
      vkGetPhysicalDeviceMemoryProperties.get(),
      vkAllocateMemory.get(),
      vkFreeMemory.get(),
      vkMapMemory.get(),
      vkUnmapMemory.get(),
      vkFlushMappedMemoryRanges.get(),
      vkInvalidateMappedMemoryRanges.get(),
      vkBindBufferMemory.get(),
      vkBindImageMemory.get(),
      vkGetBufferMemoryRequirements.get(),
      vkGetImageMemoryRequirements.get(),
      vkCreateBuffer.get(),
      vkDestroyBuffer.get(),
      vkCreateImage.get(),
      vkDestroyImage.get(),
      vkCmdCopyBuffer.get(),
      vkGetBufferMemoryRequirements2.get(),
      vkGetImageMemoryRequirements2.get(),
  };

  VmaAllocatorCreateInfo allocator_info = {
      .flags = VMA_ALLOCATOR_CREATE_EXTERNALLY_SYNCHRONIZED_BIT,
      .physicalDevice = physical_device,
      .device = device,
      // 4MB was picked for the size here by looking at memory usage of Android
      // apps and runs of DM. It seems to be a good compromise of not wasting
      // unused allocated space and not making too many small allocations. The
      // AMD allocator will start making blocks at 1/8 the max size and builds
      // up block size as needed before capping at the max set here.
      .preferredLargeHeapBlockSize = 4 * 1024 * 1024,
      .pVulkanFunctions = &functions,
      .instance = instance,
  };

  return vmaCreateAllocator(&allocator_info, pAllocator);
}

void DestroyAllocator(VmaAllocator allocator) {
  vmaDestroyAllocator(allocator);
}

VkResult AllocateMemoryForImage(VmaAllocator allocator,
                                VkImage image,
                                const VmaAllocationCreateInfo* create_info,
                                VmaAllocation* allocation,
                                VmaAllocationInfo* allocation_info) {
  return vmaAllocateMemoryForImage(allocator, image, create_info, allocation,
                                   allocation_info);
}

VkResult AllocateMemoryForBuffer(VmaAllocator allocator,
                                 VkBuffer buffer,
                                 const VmaAllocationCreateInfo* create_info,
                                 VmaAllocation* allocation,
                                 VmaAllocationInfo* allocation_info) {
  return vmaAllocateMemoryForBuffer(allocator, buffer, create_info, allocation,
                                    allocation_info);
}

VkResult CreateBuffer(VmaAllocator allocator,
                      const VkBufferCreateInfo* buffer_create_info,
                      VkMemoryPropertyFlags required_flags,
                      VkMemoryPropertyFlags preferred_flags,
                      VkBuffer* buffer,
                      VmaAllocation* allocation) {
  VmaAllocationCreateInfo allocation_create_info = {
      .requiredFlags = required_flags,
      .preferredFlags = preferred_flags,
  };

  return vmaCreateBuffer(allocator, buffer_create_info, &allocation_create_info,
                         buffer, allocation, nullptr);
}

void DestroyBuffer(VmaAllocator allocator,
                   VkBuffer buffer,
                   VmaAllocation allocation) {
  vmaDestroyBuffer(allocator, buffer, allocation);
}

VkResult MapMemory(VmaAllocator allocator,
                   VmaAllocation allocation,
                   void** data) {
  return vmaMapMemory(allocator, allocation, data);
}

void UnmapMemory(VmaAllocator allocator, VmaAllocation allocation) {
  return vmaUnmapMemory(allocator, allocation);
}

void FreeMemory(VmaAllocator allocator, VmaAllocation allocation) {
  vmaFreeMemory(allocator, allocation);
}

void FlushAllocation(VmaAllocator allocator,
                     VmaAllocation allocation,
                     VkDeviceSize offset,
                     VkDeviceSize size) {
  vmaFlushAllocation(allocator, allocation, offset, size);
}

void InvalidateAllocation(VmaAllocator allocator,
                          VmaAllocation allocation,
                          VkDeviceSize offset,
                          VkDeviceSize size) {
  vmaInvalidateAllocation(allocator, allocation, offset, size);
}

void GetAllocationInfo(VmaAllocator allocator,
                       VmaAllocation allocation,
                       VmaAllocationInfo* allocation_info) {
  vmaGetAllocationInfo(allocator, allocation, allocation_info);
}

void GetMemoryTypeProperties(VmaAllocator allocator,
                             uint32_t memory_type_index,
                             VkMemoryPropertyFlags* flags) {
  vmaGetMemoryTypeProperties(allocator, memory_type_index, flags);
}

void GetPhysicalDeviceProperties(
    VmaAllocator allocator,
    const VkPhysicalDeviceProperties** physical_device_properties) {
  vmaGetPhysicalDeviceProperties(allocator, physical_device_properties);
}

void CalculateStats(VmaAllocator allocator, VmaStats* stats) {
  vmaCalculateStats(allocator, stats);
}

}  // namespace vma
}  // namespace gpu