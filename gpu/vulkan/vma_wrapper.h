// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VMA_WRAPPER_H_
#define GPU_VULKAN_VMA_WRAPPER_H_

#include <vulkan/vulkan.h>

#include "gpu/vulkan/vulkan_export.h"

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaAllocation)

struct VmaAllocationCreateInfo;
struct VmaAllocationInfo;
struct VmaStats;

namespace gpu {
namespace vma {

VULKAN_EXPORT VkResult CreateAllocator(VkPhysicalDevice physical_device,
                                       VkDevice device,
                                       VkInstance instance,
                                       VmaAllocator* allocator);

VULKAN_EXPORT void DestroyAllocator(VmaAllocator allocator);

VULKAN_EXPORT VkResult
AllocateMemoryForImage(VmaAllocator allocator,
                       VkImage image,
                       const VmaAllocationCreateInfo* create_info,
                       VmaAllocation* allocation,
                       VmaAllocationInfo* allocation_info);

VULKAN_EXPORT VkResult
AllocateMemoryForBuffer(VmaAllocator allocator,
                        VkBuffer buffer,
                        const VmaAllocationCreateInfo* create_info,
                        VmaAllocation* allocation,
                        VmaAllocationInfo* allocationInfo);

VULKAN_EXPORT VkResult
CreateBuffer(VmaAllocator allocator,
             const VkBufferCreateInfo* buffer_create_info,
             VkMemoryPropertyFlags required_flags,
             VkMemoryPropertyFlags preferred_flags,
             VkBuffer* buffer,
             VmaAllocation* allocation);

VULKAN_EXPORT void DestroyBuffer(VmaAllocator allocator,
                                 VkBuffer buffer,
                                 VmaAllocation allocation);

VULKAN_EXPORT VkResult MapMemory(VmaAllocator allocator,
                                 VmaAllocation allocation,
                                 void** data);

VULKAN_EXPORT void UnmapMemory(VmaAllocator allocator,
                               VmaAllocation allocation);

VULKAN_EXPORT void FreeMemory(VmaAllocator allocator, VmaAllocation allocation);

VULKAN_EXPORT void FlushAllocation(VmaAllocator allocator,
                                   VmaAllocation allocation,
                                   VkDeviceSize offset,
                                   VkDeviceSize size);

VULKAN_EXPORT void InvalidateAllocation(VmaAllocator allocator,
                                        VmaAllocation allocation,
                                        VkDeviceSize offset,
                                        VkDeviceSize size);

VULKAN_EXPORT void GetAllocationInfo(VmaAllocator allocator,
                                     VmaAllocation allocation,
                                     VmaAllocationInfo* allocation_info);

VULKAN_EXPORT void GetMemoryTypeProperties(VmaAllocator allocator,
                                           uint32_t memory_type_index,
                                           VkMemoryPropertyFlags* flags);

VULKAN_EXPORT void GetPhysicalDeviceProperties(
    VmaAllocator allocator,
    const VkPhysicalDeviceProperties** physical_device_properties);

VULKAN_EXPORT void CalculateStats(VmaAllocator allocator, VmaStats* stats);

}  // namespace vma
}  // namespace gpu

#endif  // GPU_VULKAN_VMA_WRAPPER_H_
