// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_device_queue.h"

#include <unordered_set>
#include <utility>
#include <vector>

#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

VulkanDeviceQueue::VulkanDeviceQueue(VkInstance vk_instance,
                                     bool enforce_protected_memory)
    : vk_instance_(vk_instance),
      enforce_protected_memory_(enforce_protected_memory) {}

VulkanDeviceQueue::~VulkanDeviceQueue() {
  DCHECK_EQ(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE), vk_physical_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
  DCHECK_EQ(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
}

bool VulkanDeviceQueue::Initialize(
    uint32_t options,
    uint32_t max_api_version,
    const std::vector<const char*>& required_extensions,
    bool allow_protected_memory,
    const GetPresentationSupportCallback& get_presentation_support) {
  DCHECK_EQ(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE), vk_physical_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), owned_vk_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
  DCHECK_EQ(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);
  DCHECK(!enforce_protected_memory_ || allow_protected_memory);

  if (VK_NULL_HANDLE == vk_instance_)
    return false;

  VkResult result = VK_SUCCESS;

  uint32_t device_count = 0;
  result = vkEnumeratePhysicalDevices(vk_instance_, &device_count, nullptr);
  if (VK_SUCCESS != result || device_count == 0)
    return false;

  std::vector<VkPhysicalDevice> devices(device_count);
  result =
      vkEnumeratePhysicalDevices(vk_instance_, &device_count, devices.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumeratePhysicalDevices() failed: " << result;
    return false;
  }

  VkQueueFlags queue_flags = 0;
  if (options & DeviceQueueOption::GRAPHICS_QUEUE_FLAG)
    queue_flags |= VK_QUEUE_GRAPHICS_BIT;

  int device_index = -1;
  int queue_index = -1;
  for (size_t i = 0; i < devices.size(); ++i) {
    const VkPhysicalDevice& device = devices[i];
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count, nullptr);
    if (queue_count) {
      std::vector<VkQueueFamilyProperties> queue_properties(queue_count);
      vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_count,
                                               queue_properties.data());
      for (size_t n = 0; n < queue_properties.size(); ++n) {
        if ((queue_properties[n].queueFlags & queue_flags) != queue_flags)
          continue;

        if (options & DeviceQueueOption::PRESENTATION_SUPPORT_QUEUE_FLAG &&
            !get_presentation_support.Run(device, queue_properties, n)) {
          continue;
        }

        queue_index = static_cast<int>(n);
        break;
      }

      if (-1 != queue_index) {
        device_index = static_cast<int>(i);
        break;
      }
    }
  }

  if (queue_index == -1)
    return false;

  vk_physical_device_ = devices[device_index];
  vkGetPhysicalDeviceProperties(vk_physical_device_,
                                &vk_physical_device_properties_);

  vk_queue_index_ = queue_index;

  float queue_priority = 0.0f;
  VkDeviceQueueCreateInfo queue_create_info = {};
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = queue_index;
  queue_create_info.queueCount = 1;
  queue_create_info.pQueuePriorities = &queue_priority;
  queue_create_info.flags =
      allow_protected_memory ? VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT : 0;

  std::vector<const char*> enabled_layer_names;
#if DCHECK_IS_ON()
  uint32_t num_device_layers = 0;
  result = vkEnumerateDeviceLayerProperties(vk_physical_device_,
                                            &num_device_layers, nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateDeviceLayerProperties(NULL) failed: " << result;
    return false;
  }

  std::vector<VkLayerProperties> device_layers(num_device_layers);
  result = vkEnumerateDeviceLayerProperties(
      vk_physical_device_, &num_device_layers, device_layers.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateDeviceLayerProperties() failed: " << result;
    return false;
  }

  std::unordered_set<std::string> desired_layers({
      "VK_LAYER_KHRONOS_validation",
  });

  for (const VkLayerProperties& layer_property : device_layers) {
    if (desired_layers.find(layer_property.layerName) != desired_layers.end())
      enabled_layer_names.push_back(layer_property.layerName);
  }
#endif  // DCHECK_IS_ON()

  std::vector<const char*> enabled_extensions;
  enabled_extensions.insert(std::end(enabled_extensions),
                            std::begin(required_extensions),
                            std::end(required_extensions));

  uint32_t device_api_version =
      std::min(max_api_version, vk_physical_device_properties_.apiVersion);

  // Android and Fuchsia need YCbCr sampler support.
#if defined(OS_ANDROID) || defined(OS_FUCHSIA)
  if (!vkGetPhysicalDeviceFeatures2) {
    DLOG(ERROR) << "Vulkan 1.1 or VK_KHR_get_physical_device_properties2 "
                   "extension is required.";
    return false;
  }

  // Query if VkPhysicalDeviceSamplerYcbcrConversionFeatures is supported by
  // the implementation. This extension must be supported for Android and
  // Fuchsia.
  sampler_ycbcr_conversion_features_.pNext = nullptr;
  VkPhysicalDeviceFeatures2 supported_device_features_2 = {
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
      &sampler_ycbcr_conversion_features_};
  vkGetPhysicalDeviceFeatures2(vk_physical_device_,
                               &supported_device_features_2);
  if (!sampler_ycbcr_conversion_features_.samplerYcbcrConversion) {
    LOG(ERROR) << "samplerYcbcrConversion is not supported.";
    return false;
  }

  // Add VkPhysicalDeviceSamplerYcbcrConversionFeatures struct to pNext chain
  // of VkPhysicalDeviceFeatures2 to enable YCbCr sampler support.
  sampler_ycbcr_conversion_features_.pNext = enabled_device_features_2_.pNext;
  enabled_device_features_2_.pNext = &sampler_ycbcr_conversion_features_;
#endif  // defined(OS_ANDROID) || defined(OS_FUCHSIA)

  if (allow_protected_memory) {
    if (device_api_version < VK_MAKE_VERSION(1, 1, 0)) {
      DLOG(ERROR) << "Vulkan 1.1 is required for protected memory";
      return false;
    }

    protected_memory_features_.pNext = nullptr;
    VkPhysicalDeviceFeatures2 supported_device_features_2 = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        &protected_memory_features_};
    vkGetPhysicalDeviceFeatures2(vk_physical_device_,
                                 &supported_device_features_2);
    if (!protected_memory_features_.protectedMemory) {
      DLOG(ERROR) << "Protected memory is not supported";
      return false;
    }

    // Add VkPhysicalDeviceProtectedMemoryFeatures struct to pNext chain
    // of VkPhysicalDeviceFeatures2 to enable YCbCr sampler support.
    protected_memory_features_.pNext = enabled_device_features_2_.pNext;
    enabled_device_features_2_.pNext = &protected_memory_features_;
  }

  // Disable all physical device features by default.
  enabled_device_features_2_.features = {};

  VkDeviceCreateInfo device_create_info = {};
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = enabled_device_features_2_.pNext;
  device_create_info.queueCreateInfoCount = 1;
  device_create_info.pQueueCreateInfos = &queue_create_info;
  device_create_info.enabledLayerCount = enabled_layer_names.size();
  device_create_info.ppEnabledLayerNames = enabled_layer_names.data();
  device_create_info.enabledExtensionCount = enabled_extensions.size();
  device_create_info.ppEnabledExtensionNames = enabled_extensions.data();
  device_create_info.pEnabledFeatures = &enabled_device_features_2_.features;

  result = vkCreateDevice(vk_physical_device_, &device_create_info, nullptr,
                          &owned_vk_device_);
  if (VK_SUCCESS != result)
    return false;

  enabled_extensions_ = gfx::ExtensionSet(std::begin(enabled_extensions),
                                          std::end(enabled_extensions));

  if (!gpu::GetVulkanFunctionPointers()->BindDeviceFunctionPointers(
          owned_vk_device_, device_api_version, enabled_extensions_)) {
    vkDestroyDevice(owned_vk_device_, nullptr);
    owned_vk_device_ = VK_NULL_HANDLE;
    return false;
  }

  vk_device_ = owned_vk_device_;

  if (allow_protected_memory) {
    VkDeviceQueueInfo2 queue_info2 = {};
    queue_info2.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2;
    queue_info2.flags = VK_DEVICE_QUEUE_CREATE_PROTECTED_BIT;
    queue_info2.queueFamilyIndex = queue_index;
    queue_info2.queueIndex = 0;
    vkGetDeviceQueue2(vk_device_, &queue_info2, &vk_queue_);
  } else {
    vkGetDeviceQueue(vk_device_, queue_index, 0, &vk_queue_);
  }

  cleanup_helper_ = std::make_unique<VulkanFenceHelper>(this);

  allow_protected_memory_ = allow_protected_memory;

  return true;
}

bool VulkanDeviceQueue::InitializeForWebView(
    VkPhysicalDevice vk_physical_device,
    VkDevice vk_device,
    VkQueue vk_queue,
    uint32_t vk_queue_index,
    gfx::ExtensionSet enabled_extensions) {
  DCHECK_EQ(static_cast<VkPhysicalDevice>(VK_NULL_HANDLE), vk_physical_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), owned_vk_device_);
  DCHECK_EQ(static_cast<VkDevice>(VK_NULL_HANDLE), vk_device_);
  DCHECK_EQ(static_cast<VkQueue>(VK_NULL_HANDLE), vk_queue_);

  vk_physical_device_ = vk_physical_device;
  vk_device_ = vk_device;
  vk_queue_ = vk_queue;
  vk_queue_index_ = vk_queue_index;
  enabled_extensions_ = std::move(enabled_extensions);

  cleanup_helper_ = std::make_unique<VulkanFenceHelper>(this);
  return true;
}

void VulkanDeviceQueue::Destroy() {
  if (cleanup_helper_) {
    cleanup_helper_->Destroy();
    cleanup_helper_.reset();
  }

  if (VK_NULL_HANDLE != owned_vk_device_) {
    vkDestroyDevice(owned_vk_device_, nullptr);
    owned_vk_device_ = VK_NULL_HANDLE;
  }
  vk_device_ = VK_NULL_HANDLE;
  vk_queue_ = VK_NULL_HANDLE;
  vk_queue_index_ = 0;
  vk_physical_device_ = VK_NULL_HANDLE;
}

std::unique_ptr<VulkanCommandPool> VulkanDeviceQueue::CreateCommandPool() {
  std::unique_ptr<VulkanCommandPool> command_pool(new VulkanCommandPool(this));
  if (!command_pool->Initialize(enforce_protected_memory_))
    return nullptr;

  return command_pool;
}

}  // namespace gpu
