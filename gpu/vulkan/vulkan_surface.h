// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_SURFACE_H_
#define GPU_VULKAN_VULKAN_SURFACE_H_

#include <vulkan/vulkan.h>

#include "base/callback.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_export.h"
#include "gpu/vulkan/vulkan_swap_chain.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/swap_result.h"

namespace gpu {

class VulkanDeviceQueue;
class VulkanSwapChain;

class VULKAN_EXPORT VulkanSurface {
 public:
  // Minimum bit depth of surface.
  enum Format {
    FORMAT_RGBA_32,
    FORMAT_RGB_16,

    NUM_SURFACE_FORMATS,
    DEFAULT_SURFACE_FORMAT = FORMAT_RGBA_32
  };

  VulkanSurface(VkInstance vk_instance, VkSurfaceKHR surface);

  virtual ~VulkanSurface();

  bool Initialize(VulkanDeviceQueue* device_queue,
                  VulkanSurface::Format format);
  // Destroy() should be called when all related GPU tasks have been finished.
  void Destroy();

  gfx::SwapResult SwapBuffers();

  VulkanSwapChain* GetSwapChain();
  uint32_t swap_chain_generation() const { return swap_chain_generation_; }

  void Finish();

  virtual bool SetSize(const gfx::Size& size);

  const gfx::Size& size() const { return size_; }
  VkSurfaceFormatKHR surface_format() const { return surface_format_; }

 private:
  bool CreateSwapChain(const gfx::Size& new_size);

  const VkInstance vk_instance_;
  gfx::Size size_;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkSurfaceFormatKHR surface_format_ = {};
  VulkanDeviceQueue* device_queue_ = nullptr;

  // The generation of |swap_chain_|, it will be increasted if a new
  // |swap_chain_| is created due to resizing, etec.
  uint32_t swap_chain_generation_ = 0u;
  std::unique_ptr<VulkanSwapChain> swap_chain_;

  DISALLOW_COPY_AND_ASSIGN(VulkanSurface);
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_SURFACE_H_
