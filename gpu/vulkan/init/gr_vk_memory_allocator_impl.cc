// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/init/gr_vk_memory_allocator_impl.h"

#include <vk_mem_alloc.h>

#include "base/trace_event/trace_event.h"
#include "gpu/vulkan/vma_wrapper.h"
#include "gpu/vulkan/vulkan_device_queue.h"

namespace gpu {

namespace {

class GrVkMemoryAllocatorImpl : public GrVkMemoryAllocator {
 public:
  explicit GrVkMemoryAllocatorImpl(VmaAllocator allocator)
      : allocator_(allocator) {}
  ~GrVkMemoryAllocatorImpl() override = default;

  GrVkMemoryAllocatorImpl(const GrVkMemoryAllocatorImpl&) = delete;
  GrVkMemoryAllocatorImpl& operator=(const GrVkMemoryAllocatorImpl&) = delete;

 private:
  bool allocateMemoryForImage(VkImage image,
                              AllocationPropertyFlags flags,
                              GrVkBackendMemory* backend_memory) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::allocateMemoryForImage");
    VmaAllocationCreateInfo info;
    info.flags = 0;
    info.usage = VMA_MEMORY_USAGE_UNKNOWN;
    info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    info.preferredFlags = 0;
    info.memoryTypeBits = 0;
    info.pool = VK_NULL_HANDLE;
    info.pUserData = nullptr;

    if (AllocationPropertyFlags::kDedicatedAllocation & flags) {
      info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    if (AllocationPropertyFlags::kLazyAllocation & flags) {
      info.preferredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }

    if (AllocationPropertyFlags::kProtected & flags) {
      info.requiredFlags |= VK_MEMORY_PROPERTY_PROTECTED_BIT;
    }

    VmaAllocation allocation;
    VkResult result = vma::AllocateMemoryForImage(allocator_, image, &info,
                                                  &allocation, nullptr);
    if (VK_SUCCESS != result) {
      return false;
    }
    *backend_memory = reinterpret_cast<GrVkBackendMemory>(allocation);
    return true;
  }

  bool allocateMemoryForBuffer(VkBuffer buffer,
                               BufferUsage usage,
                               AllocationPropertyFlags flags,
                               GrVkBackendMemory* backend_memory) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::allocateMemoryForBuffer");
    VmaAllocationCreateInfo info;
    info.flags = 0;
    info.usage = VMA_MEMORY_USAGE_UNKNOWN;
    info.memoryTypeBits = 0;
    info.pool = VK_NULL_HANDLE;
    info.pUserData = nullptr;

    switch (usage) {
      case BufferUsage::kGpuOnly:
        info.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        info.preferredFlags = 0;
        break;
      case BufferUsage::kCpuOnly:
        info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        info.preferredFlags = VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
      case BufferUsage::kCpuWritesGpuReads:
        // First attempt to try memory is also cached
        info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        break;
      case BufferUsage::kGpuWritesCpuReads:
        info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        info.preferredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                              VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        break;
    }

    if (AllocationPropertyFlags::kDedicatedAllocation & flags) {
      info.flags |= VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    if ((AllocationPropertyFlags::kLazyAllocation & flags) &&
        BufferUsage::kGpuOnly == usage) {
      info.preferredFlags |= VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
    }

    if (AllocationPropertyFlags::kPersistentlyMapped & flags) {
      SkASSERT(BufferUsage::kGpuOnly != usage);
      info.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VmaAllocation allocation;
    VkResult result = vma::AllocateMemoryForBuffer(allocator_, buffer, &info,
                                                   &allocation, nullptr);
    if (VK_SUCCESS != result) {
      if (usage == BufferUsage::kCpuWritesGpuReads) {
        // We try again but this time drop the requirement for cached
        info.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        result = vma::AllocateMemoryForBuffer(allocator_, buffer, &info,
                                              &allocation, nullptr);
      }
    }
    if (VK_SUCCESS != result) {
      return false;
    }

    *backend_memory = reinterpret_cast<GrVkBackendMemory>(allocation);
    return true;
  }

  void freeMemory(const GrVkBackendMemory& memory) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::freeMemory");
    vma::FreeMemory(allocator_, reinterpret_cast<const VmaAllocation>(memory));
  }

  void getAllocInfo(const GrVkBackendMemory& memory,
                    GrVkAlloc* alloc) const override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::getAllocInfo");
    const VmaAllocation allocation =
        reinterpret_cast<const VmaAllocation>(memory);
    VmaAllocationInfo vma_info;
    vma::GetAllocationInfo(allocator_, allocation, &vma_info);

    VkMemoryPropertyFlags mem_flags;
    vma::GetMemoryTypeProperties(allocator_, vma_info.memoryType, &mem_flags);

    uint32_t flags = 0;
    if (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT & mem_flags) {
      flags |= GrVkAlloc::kMappable_Flag;
    }
    if (!SkToBool(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT & mem_flags)) {
      flags |= GrVkAlloc::kNoncoherent_Flag;
    }

    alloc->fMemory = vma_info.deviceMemory;
    alloc->fOffset = vma_info.offset;
    alloc->fSize = vma_info.size;
    alloc->fFlags = flags;
    alloc->fBackendMemory = memory;
  }

  void* mapMemory(const GrVkBackendMemory& memory) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::mapMemory");
    const VmaAllocation allocation =
        reinterpret_cast<const VmaAllocation>(memory);
    void* data;
    return vma::MapMemory(allocator_, allocation, &data) == VK_SUCCESS
               ? data
               : nullptr;
  }

  void unmapMemory(const GrVkBackendMemory& memory) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::unmapMemory");
    const VmaAllocation allocation =
        reinterpret_cast<const VmaAllocation>(memory);
    vma::UnmapMemory(allocator_, allocation);
  }

  void flushMappedMemory(const GrVkBackendMemory& memory,
                         VkDeviceSize offset,
                         VkDeviceSize size) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::flushMappedMemory");
    const VmaAllocation allocation =
        reinterpret_cast<const VmaAllocation>(memory);
    vma::FlushAllocation(allocator_, allocation, offset, size);
  }

  void invalidateMappedMemory(const GrVkBackendMemory& memory,
                              VkDeviceSize offset,
                              VkDeviceSize size) override {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("gpu.vulkan.vma"),
                 "GrVkMemoryAllocatorImpl::invalidateMappedMemory");
    const VmaAllocation allocation =
        reinterpret_cast<const VmaAllocation>(memory);
    vma::InvalidateAllocation(allocator_, allocation, offset, size);
  }

  uint64_t totalUsedMemory() const override {
    VmaStats stats;
    vma::CalculateStats(allocator_, &stats);
    return stats.total.usedBytes;
  }

  uint64_t totalAllocatedMemory() const override {
    VmaStats stats;
    vma::CalculateStats(allocator_, &stats);
    return stats.total.usedBytes + stats.total.unusedBytes;
  }

  const VmaAllocator allocator_;
};

}  // namespace

sk_sp<GrVkMemoryAllocator> CreateGrVkMemoryAllocator(
    VulkanDeviceQueue* device_queue) {
  return sk_make_sp<GrVkMemoryAllocatorImpl>(device_queue->vma_allocator());
}

}  // namespace gpu