// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_SWAP_CHAIN_H_
#define GPU_VULKAN_VULKAN_SWAP_CHAIN_H_

#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

#include "base/logging.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/swap_result.h"

namespace gpu {

class VulkanCommandBuffer;
class VulkanCommandPool;
class VulkanDeviceQueue;

class VULKAN_EXPORT VulkanSwapChain {
 public:
  class VULKAN_EXPORT ScopedWrite {
   public:
    explicit ScopedWrite(VulkanSwapChain* swap_chain);
    ~ScopedWrite();

    VkImage image() const { return image_; }
    uint32_t image_index() const { return image_index_; }
    VkImageLayout image_layout() const { return image_layout_; }
    void set_image_layout(VkImageLayout layout) { image_layout_ = layout; }

    // Take the begin write semaphore. The ownership of the semaphore will be
    // transferred to the caller.
    VkSemaphore TakeBeginSemaphore();

    // Set the end write semaphore. The ownership of the semaphore will be
    // transferred to ScopedWrite.
    void SetEndSemaphore(VkSemaphore);

   private:
    VulkanSwapChain* const swap_chain_;
    VkImage image_ = VK_NULL_HANDLE;
    uint32_t image_index_ = 0;
    VkImageLayout image_layout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    VkSemaphore begin_semaphore_ = VK_NULL_HANDLE;
    VkSemaphore end_semaphore_ = VK_NULL_HANDLE;

    DISALLOW_COPY_AND_ASSIGN(ScopedWrite);
  };

  VulkanSwapChain();
  ~VulkanSwapChain();

  bool Initialize(VulkanDeviceQueue* device_queue,
                  VkSurfaceKHR surface,
                  const VkSurfaceCapabilitiesKHR& surface_caps,
                  const VkSurfaceFormatKHR& surface_format,
                  std::unique_ptr<VulkanSwapChain> old_swap_chain);
  // Destroy() should be called when all related GPU tasks have been finished.
  void Destroy();
  gfx::SwapResult SwapBuffers();

  uint32_t num_images() const { return static_cast<uint32_t>(images_.size()); }
  uint32_t current_image() const { return current_image_; }
  const gfx::Size& size() const { return size_; }

 private:
  bool InitializeSwapChain(VkSurfaceKHR surface,
                           const VkSurfaceCapabilitiesKHR& surface_caps,
                           const VkSurfaceFormatKHR& surface_format,
                           std::unique_ptr<VulkanSwapChain> old_swap_chain);
  void DestroySwapChain();

  bool InitializeSwapImages(const VkSurfaceCapabilitiesKHR& surface_caps,
                            const VkSurfaceFormatKHR& surface_format);
  void DestroySwapImages();
  void BeginWriteCurrentImage(VkImage* image,
                              uint32_t* image_index,
                              VkImageLayout* layout,
                              VkSemaphore* semaphore);
  void EndWriteCurrentImage(VkImageLayout layout, VkSemaphore semaphore);

  VulkanDeviceQueue* device_queue_;
  VkSwapchainKHR swap_chain_ = VK_NULL_HANDLE;

  std::unique_ptr<VulkanCommandPool> command_pool_;

  gfx::Size size_;

  struct ImageData {
    ImageData();
    ImageData(ImageData&& other);
    ~ImageData();

    ImageData& operator=(ImageData&& other);

    VkImage image = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::unique_ptr<VulkanCommandBuffer> command_buffer;
  };
  std::vector<ImageData> images_;
  uint32_t current_image_ = 0;
  bool is_writing_ = false;
  VkSemaphore begin_write_semaphore_ = VK_NULL_HANDLE;
  VkSemaphore end_write_semaphore_ = VK_NULL_HANDLE;

  DISALLOW_COPY_AND_ASSIGN(VulkanSwapChain);
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_SWAP_CHAIN_H_
