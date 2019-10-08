// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_VULKAN_INFO_H_
#define GPU_VULKAN_VULKAN_INFO_H_

#include <vulkan/vulkan.h>
#include <vector>

#include "base/macros.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/extension_set.h"

namespace gpu {

class VULKAN_EXPORT VulkanInfo {
 public:
  VulkanInfo();
  ~VulkanInfo();

  class PhysicalDeviceInfo {
   public:
    PhysicalDeviceInfo();
    PhysicalDeviceInfo(const PhysicalDeviceInfo& other);
    ~PhysicalDeviceInfo();
    PhysicalDeviceInfo& operator=(const PhysicalDeviceInfo& other);

    VkPhysicalDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties properties = {};
    std::vector<VkLayerProperties> layers;

    VkPhysicalDeviceFeatures features = {};
    // Extended physical device features:
    bool feature_sampler_ycbcr_conversion = false;
    bool feature_protected_memory = false;

    std::vector<VkQueueFamilyProperties> queue_families;
  };

  uint32_t api_version = VK_MAKE_VERSION(1, 0, 0);
  uint32_t used_api_version = VK_MAKE_VERSION(1, 0, 0);
  std::vector<VkExtensionProperties> instance_extensions;
  std::vector<const char*> enabled_instance_extensions;
  std::vector<VkLayerProperties> instance_layers;
  std::vector<PhysicalDeviceInfo> physical_devices;
};

}  // namespace gpu

#endif  // GPU_VULKAN_VULKAN_INFO_H_
