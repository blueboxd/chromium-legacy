// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#ifndef GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
#define GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_

#include <vulkan/vulkan.h>

#include "base/compiler_specific.h"
#include "base/native_library.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_export.h"
#include "ui/gfx/extension_set.h"

#if defined(OS_ANDROID)
#include <vulkan/vulkan_android.h>
#endif

#if defined(OS_FUCHSIA)
#include <zircon/types.h>
// <vulkan/vulkan_fuchsia.h> must be included after <zircon/types.h>
#include <vulkan/vulkan_fuchsia.h>

#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if defined(USE_VULKAN_XLIB)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#endif

namespace gpu {

class VulkanFunctionPointers;

VULKAN_EXPORT VulkanFunctionPointers* GetVulkanFunctionPointers();

class VulkanFunctionPointers {
 public:
  VulkanFunctionPointers();
  ~VulkanFunctionPointers();

  VULKAN_EXPORT bool BindUnassociatedFunctionPointers();

  // These functions assume that vkGetInstanceProcAddr has been populated.
  VULKAN_EXPORT bool BindInstanceFunctionPointers(
      VkInstance vk_instance,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  // These functions assume that vkGetDeviceProcAddr has been populated.
  VULKAN_EXPORT bool BindDeviceFunctionPointers(
      VkDevice vk_device,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  bool HasEnumerateInstanceVersion() const {
    return !!vkEnumerateInstanceVersionFn_;
  }

  base::NativeLibrary vulkan_loader_library() const {
    return vulkan_loader_library_;
  }

  void set_vulkan_loader_library(base::NativeLibrary library) {
    vulkan_loader_library_ = library;
  }

 private:
  base::NativeLibrary vulkan_loader_library_ = nullptr;

  // Unassociated functions
  PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersionFn_ = nullptr;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddrFn_ = nullptr;
  PFN_vkCreateInstance vkCreateInstanceFn_ = nullptr;
  PFN_vkEnumerateInstanceExtensionProperties
      vkEnumerateInstanceExtensionPropertiesFn_ = nullptr;
  PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerPropertiesFn_ =
      nullptr;

  // Instance functions
  PFN_vkCreateDevice vkCreateDeviceFn_ = nullptr;
  PFN_vkDestroyInstance vkDestroyInstanceFn_ = nullptr;
  PFN_vkEnumerateDeviceExtensionProperties
      vkEnumerateDeviceExtensionPropertiesFn_ = nullptr;
  PFN_vkEnumerateDeviceLayerProperties vkEnumerateDeviceLayerPropertiesFn_ =
      nullptr;
  PFN_vkEnumeratePhysicalDevices vkEnumeratePhysicalDevicesFn_ = nullptr;
  PFN_vkGetDeviceProcAddr vkGetDeviceProcAddrFn_ = nullptr;
  PFN_vkGetPhysicalDeviceFeatures vkGetPhysicalDeviceFeaturesFn_ = nullptr;
  PFN_vkGetPhysicalDeviceFormatProperties
      vkGetPhysicalDeviceFormatPropertiesFn_ = nullptr;
  PFN_vkGetPhysicalDeviceMemoryProperties
      vkGetPhysicalDeviceMemoryPropertiesFn_ = nullptr;
  PFN_vkGetPhysicalDeviceProperties vkGetPhysicalDevicePropertiesFn_ = nullptr;
  PFN_vkGetPhysicalDeviceQueueFamilyProperties
      vkGetPhysicalDeviceQueueFamilyPropertiesFn_ = nullptr;

#if DCHECK_IS_ON()
  PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXTFn_ =
      nullptr;
  PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXTFn_ =
      nullptr;
#endif  // DCHECK_IS_ON()

  PFN_vkDestroySurfaceKHR vkDestroySurfaceKHRFn_ = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
      vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn_ = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
      vkGetPhysicalDeviceSurfaceFormatsKHRFn_ = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR
      vkGetPhysicalDeviceSurfaceSupportKHRFn_ = nullptr;

#if defined(USE_VULKAN_XLIB)
  PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHRFn_ = nullptr;
  PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR
      vkGetPhysicalDeviceXlibPresentationSupportKHRFn_ = nullptr;
#endif  // defined(USE_VULKAN_XLIB)

#if defined(OS_ANDROID)
  PFN_vkCreateAndroidSurfaceKHR vkCreateAndroidSurfaceKHRFn_ = nullptr;
#endif  // defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
  PFN_vkCreateImagePipeSurfaceFUCHSIA vkCreateImagePipeSurfaceFUCHSIAFn_ =
      nullptr;
#endif  // defined(OS_FUCHSIA)

  PFN_vkGetPhysicalDeviceImageFormatProperties2
      vkGetPhysicalDeviceImageFormatProperties2Fn_ = nullptr;

  PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2Fn_ = nullptr;

  // Device functions
  PFN_vkAllocateCommandBuffers vkAllocateCommandBuffersFn_ = nullptr;
  PFN_vkAllocateDescriptorSets vkAllocateDescriptorSetsFn_ = nullptr;
  PFN_vkAllocateMemory vkAllocateMemoryFn_ = nullptr;
  PFN_vkBeginCommandBuffer vkBeginCommandBufferFn_ = nullptr;
  PFN_vkBindBufferMemory vkBindBufferMemoryFn_ = nullptr;
  PFN_vkBindImageMemory vkBindImageMemoryFn_ = nullptr;
  PFN_vkCmdBeginRenderPass vkCmdBeginRenderPassFn_ = nullptr;
  PFN_vkCmdCopyBufferToImage vkCmdCopyBufferToImageFn_ = nullptr;
  PFN_vkCmdEndRenderPass vkCmdEndRenderPassFn_ = nullptr;
  PFN_vkCmdExecuteCommands vkCmdExecuteCommandsFn_ = nullptr;
  PFN_vkCmdNextSubpass vkCmdNextSubpassFn_ = nullptr;
  PFN_vkCmdPipelineBarrier vkCmdPipelineBarrierFn_ = nullptr;
  PFN_vkCreateBuffer vkCreateBufferFn_ = nullptr;
  PFN_vkCreateCommandPool vkCreateCommandPoolFn_ = nullptr;
  PFN_vkCreateDescriptorPool vkCreateDescriptorPoolFn_ = nullptr;
  PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayoutFn_ = nullptr;
  PFN_vkCreateFence vkCreateFenceFn_ = nullptr;
  PFN_vkCreateFramebuffer vkCreateFramebufferFn_ = nullptr;
  PFN_vkCreateImage vkCreateImageFn_ = nullptr;
  PFN_vkCreateImageView vkCreateImageViewFn_ = nullptr;
  PFN_vkCreateRenderPass vkCreateRenderPassFn_ = nullptr;
  PFN_vkCreateSampler vkCreateSamplerFn_ = nullptr;
  PFN_vkCreateSemaphore vkCreateSemaphoreFn_ = nullptr;
  PFN_vkCreateShaderModule vkCreateShaderModuleFn_ = nullptr;
  PFN_vkDestroyBuffer vkDestroyBufferFn_ = nullptr;
  PFN_vkDestroyCommandPool vkDestroyCommandPoolFn_ = nullptr;
  PFN_vkDestroyDescriptorPool vkDestroyDescriptorPoolFn_ = nullptr;
  PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayoutFn_ = nullptr;
  PFN_vkDestroyDevice vkDestroyDeviceFn_ = nullptr;
  PFN_vkDestroyFence vkDestroyFenceFn_ = nullptr;
  PFN_vkDestroyFramebuffer vkDestroyFramebufferFn_ = nullptr;
  PFN_vkDestroyImage vkDestroyImageFn_ = nullptr;
  PFN_vkDestroyImageView vkDestroyImageViewFn_ = nullptr;
  PFN_vkDestroyRenderPass vkDestroyRenderPassFn_ = nullptr;
  PFN_vkDestroySampler vkDestroySamplerFn_ = nullptr;
  PFN_vkDestroySemaphore vkDestroySemaphoreFn_ = nullptr;
  PFN_vkDestroyShaderModule vkDestroyShaderModuleFn_ = nullptr;
  PFN_vkDeviceWaitIdle vkDeviceWaitIdleFn_ = nullptr;
  PFN_vkEndCommandBuffer vkEndCommandBufferFn_ = nullptr;
  PFN_vkFreeCommandBuffers vkFreeCommandBuffersFn_ = nullptr;
  PFN_vkFreeDescriptorSets vkFreeDescriptorSetsFn_ = nullptr;
  PFN_vkFreeMemory vkFreeMemoryFn_ = nullptr;
  PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirementsFn_ = nullptr;
  PFN_vkGetDeviceQueue vkGetDeviceQueueFn_ = nullptr;
  PFN_vkGetFenceStatus vkGetFenceStatusFn_ = nullptr;
  PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirementsFn_ = nullptr;
  PFN_vkMapMemory vkMapMemoryFn_ = nullptr;
  PFN_vkQueueSubmit vkQueueSubmitFn_ = nullptr;
  PFN_vkQueueWaitIdle vkQueueWaitIdleFn_ = nullptr;
  PFN_vkResetCommandBuffer vkResetCommandBufferFn_ = nullptr;
  PFN_vkResetFences vkResetFencesFn_ = nullptr;
  PFN_vkUnmapMemory vkUnmapMemoryFn_ = nullptr;
  PFN_vkUpdateDescriptorSets vkUpdateDescriptorSetsFn_ = nullptr;
  PFN_vkWaitForFences vkWaitForFencesFn_ = nullptr;

  PFN_vkGetDeviceQueue2 vkGetDeviceQueue2Fn_ = nullptr;
  PFN_vkGetImageMemoryRequirements2 vkGetImageMemoryRequirements2Fn_ = nullptr;

#if defined(OS_ANDROID)
  PFN_vkGetAndroidHardwareBufferPropertiesANDROID
      vkGetAndroidHardwareBufferPropertiesANDROIDFn_ = nullptr;
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  PFN_vkGetSemaphoreFdKHR vkGetSemaphoreFdKHRFn_ = nullptr;
  PFN_vkImportSemaphoreFdKHR vkImportSemaphoreFdKHRFn_ = nullptr;
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  PFN_vkGetMemoryFdKHR vkGetMemoryFdKHRFn_ = nullptr;
  PFN_vkGetMemoryFdPropertiesKHR vkGetMemoryFdPropertiesKHRFn_ = nullptr;
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
  PFN_vkImportSemaphoreZirconHandleFUCHSIA
      vkImportSemaphoreZirconHandleFUCHSIAFn_ = nullptr;
  PFN_vkGetSemaphoreZirconHandleFUCHSIA vkGetSemaphoreZirconHandleFUCHSIAFn_ =
      nullptr;
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  PFN_vkGetMemoryZirconHandleFUCHSIA vkGetMemoryZirconHandleFUCHSIAFn_ =
      nullptr;
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  PFN_vkCreateBufferCollectionFUCHSIA vkCreateBufferCollectionFUCHSIAFn_ =
      nullptr;
  PFN_vkSetBufferCollectionConstraintsFUCHSIA
      vkSetBufferCollectionConstraintsFUCHSIAFn_ = nullptr;
  PFN_vkGetBufferCollectionPropertiesFUCHSIA
      vkGetBufferCollectionPropertiesFUCHSIAFn_ = nullptr;
  PFN_vkDestroyBufferCollectionFUCHSIA vkDestroyBufferCollectionFUCHSIAFn_ =
      nullptr;
#endif  // defined(OS_FUCHSIA)

  PFN_vkAcquireNextImageKHR vkAcquireNextImageKHRFn_ = nullptr;
  PFN_vkCreateSwapchainKHR vkCreateSwapchainKHRFn_ = nullptr;
  PFN_vkDestroySwapchainKHR vkDestroySwapchainKHRFn_ = nullptr;
  PFN_vkGetSwapchainImagesKHR vkGetSwapchainImagesKHRFn_ = nullptr;
  PFN_vkQueuePresentKHR vkQueuePresentKHRFn_ = nullptr;

 public:
#define DEFINE_METHOD(name)                                    \
  template <typename... Types>                                 \
  NO_SANITIZE("cfi-icall")                                     \
  auto name##Fn(Types... args)->decltype(name##Fn_(args...)) { \
    return name##Fn_(args...);                                 \
  }

  // Unassociated functions
  DEFINE_METHOD(vkGetInstanceProcAddr)
  DEFINE_METHOD(vkEnumerateInstanceVersion)

  DEFINE_METHOD(vkCreateInstance)
  DEFINE_METHOD(vkEnumerateInstanceExtensionProperties)
  DEFINE_METHOD(vkEnumerateInstanceLayerProperties)

  // Instance functions
  DEFINE_METHOD(vkCreateDevice)
  DEFINE_METHOD(vkDestroyInstance)
  DEFINE_METHOD(vkEnumerateDeviceExtensionProperties)
  DEFINE_METHOD(vkEnumerateDeviceLayerProperties)
  DEFINE_METHOD(vkEnumeratePhysicalDevices)
  DEFINE_METHOD(vkGetDeviceProcAddr)
  DEFINE_METHOD(vkGetPhysicalDeviceFeatures)
  DEFINE_METHOD(vkGetPhysicalDeviceFormatProperties)
  DEFINE_METHOD(vkGetPhysicalDeviceMemoryProperties)
  DEFINE_METHOD(vkGetPhysicalDeviceProperties)
  DEFINE_METHOD(vkGetPhysicalDeviceQueueFamilyProperties)

#if DCHECK_IS_ON()
  DEFINE_METHOD(vkCreateDebugReportCallbackEXT)
  DEFINE_METHOD(vkDestroyDebugReportCallbackEXT)
#endif  // DCHECK_IS_ON()

  DEFINE_METHOD(vkDestroySurfaceKHR)
  DEFINE_METHOD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
  DEFINE_METHOD(vkGetPhysicalDeviceSurfaceFormatsKHR)
  DEFINE_METHOD(vkGetPhysicalDeviceSurfaceSupportKHR)

#if defined(USE_VULKAN_XLIB)
  DEFINE_METHOD(vkCreateXlibSurfaceKHR)
  DEFINE_METHOD(vkGetPhysicalDeviceXlibPresentationSupportKHR)
#endif  // defined(USE_VULKAN_XLIB)

#if defined(OS_ANDROID)
  DEFINE_METHOD(vkCreateAndroidSurfaceKHR)
#endif  // defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
  DEFINE_METHOD(vkCreateImagePipeSurfaceFUCHSIA)
#endif  // defined(OS_FUCHSIA)

  DEFINE_METHOD(vkGetPhysicalDeviceImageFormatProperties2)

  DEFINE_METHOD(vkGetPhysicalDeviceFeatures2)

  // Device functions
  DEFINE_METHOD(vkAllocateCommandBuffers)
  DEFINE_METHOD(vkAllocateDescriptorSets)
  DEFINE_METHOD(vkAllocateMemory)
  DEFINE_METHOD(vkBeginCommandBuffer)
  DEFINE_METHOD(vkBindBufferMemory)
  DEFINE_METHOD(vkBindImageMemory)
  DEFINE_METHOD(vkCmdBeginRenderPass)
  DEFINE_METHOD(vkCmdCopyBufferToImage)
  DEFINE_METHOD(vkCmdEndRenderPass)
  DEFINE_METHOD(vkCmdExecuteCommands)
  DEFINE_METHOD(vkCmdNextSubpass)
  DEFINE_METHOD(vkCmdPipelineBarrier)
  DEFINE_METHOD(vkCreateBuffer)
  DEFINE_METHOD(vkCreateCommandPool)
  DEFINE_METHOD(vkCreateDescriptorPool)
  DEFINE_METHOD(vkCreateDescriptorSetLayout)
  DEFINE_METHOD(vkCreateFence)
  DEFINE_METHOD(vkCreateFramebuffer)
  DEFINE_METHOD(vkCreateImage)
  DEFINE_METHOD(vkCreateImageView)
  DEFINE_METHOD(vkCreateRenderPass)
  DEFINE_METHOD(vkCreateSampler)
  DEFINE_METHOD(vkCreateSemaphore)
  DEFINE_METHOD(vkCreateShaderModule)
  DEFINE_METHOD(vkDestroyBuffer)
  DEFINE_METHOD(vkDestroyCommandPool)
  DEFINE_METHOD(vkDestroyDescriptorPool)
  DEFINE_METHOD(vkDestroyDescriptorSetLayout)
  DEFINE_METHOD(vkDestroyDevice)
  DEFINE_METHOD(vkDestroyFence)
  DEFINE_METHOD(vkDestroyFramebuffer)
  DEFINE_METHOD(vkDestroyImage)
  DEFINE_METHOD(vkDestroyImageView)
  DEFINE_METHOD(vkDestroyRenderPass)
  DEFINE_METHOD(vkDestroySampler)
  DEFINE_METHOD(vkDestroySemaphore)
  DEFINE_METHOD(vkDestroyShaderModule)
  DEFINE_METHOD(vkDeviceWaitIdle)
  DEFINE_METHOD(vkEndCommandBuffer)
  DEFINE_METHOD(vkFreeCommandBuffers)
  DEFINE_METHOD(vkFreeDescriptorSets)
  DEFINE_METHOD(vkFreeMemory)
  DEFINE_METHOD(vkGetBufferMemoryRequirements)
  DEFINE_METHOD(vkGetDeviceQueue)
  DEFINE_METHOD(vkGetFenceStatus)
  DEFINE_METHOD(vkGetImageMemoryRequirements)
  DEFINE_METHOD(vkMapMemory)
  DEFINE_METHOD(vkQueueSubmit)
  DEFINE_METHOD(vkQueueWaitIdle)
  DEFINE_METHOD(vkResetCommandBuffer)
  DEFINE_METHOD(vkResetFences)
  DEFINE_METHOD(vkUnmapMemory)
  DEFINE_METHOD(vkUpdateDescriptorSets)
  DEFINE_METHOD(vkWaitForFences)

  DEFINE_METHOD(vkGetDeviceQueue2)
  DEFINE_METHOD(vkGetImageMemoryRequirements2)

#if defined(OS_ANDROID)
  DEFINE_METHOD(vkGetAndroidHardwareBufferPropertiesANDROID)
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  DEFINE_METHOD(vkGetSemaphoreFdKHR)
  DEFINE_METHOD(vkImportSemaphoreFdKHR)
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  DEFINE_METHOD(vkGetMemoryFdKHR)
  DEFINE_METHOD(vkGetMemoryFdPropertiesKHR)
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
  DEFINE_METHOD(vkImportSemaphoreZirconHandleFUCHSIA)
  DEFINE_METHOD(vkGetSemaphoreZirconHandleFUCHSIA)
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  DEFINE_METHOD(vkGetMemoryZirconHandleFUCHSIA)
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  DEFINE_METHOD(vkCreateBufferCollectionFUCHSIA)
  DEFINE_METHOD(vkSetBufferCollectionConstraintsFUCHSIA)
  DEFINE_METHOD(vkGetBufferCollectionPropertiesFUCHSIA)
  DEFINE_METHOD(vkDestroyBufferCollectionFUCHSIA)
#endif  // defined(OS_FUCHSIA)

  DEFINE_METHOD(vkAcquireNextImageKHR)
  DEFINE_METHOD(vkCreateSwapchainKHR)
  DEFINE_METHOD(vkDestroySwapchainKHR)
  DEFINE_METHOD(vkGetSwapchainImagesKHR)
  DEFINE_METHOD(vkQueuePresentKHR)

#undef DEFINE_METHOD
};

}  // namespace gpu

// Unassociated functions
#define vkGetInstanceProcAddr \
  gpu::GetVulkanFunctionPointers()->vkGetInstanceProcAddrFn
#define vkEnumerateInstanceVersion \
  gpu::GetVulkanFunctionPointers()->vkEnumerateInstanceVersionFn

#define vkCreateInstance gpu::GetVulkanFunctionPointers()->vkCreateInstanceFn
#define vkEnumerateInstanceExtensionProperties \
  gpu::GetVulkanFunctionPointers()->vkEnumerateInstanceExtensionPropertiesFn
#define vkEnumerateInstanceLayerProperties \
  gpu::GetVulkanFunctionPointers()->vkEnumerateInstanceLayerPropertiesFn

// Instance functions
#define vkCreateDevice gpu::GetVulkanFunctionPointers()->vkCreateDeviceFn
#define vkDestroyInstance gpu::GetVulkanFunctionPointers()->vkDestroyInstanceFn
#define vkEnumerateDeviceExtensionProperties \
  gpu::GetVulkanFunctionPointers()->vkEnumerateDeviceExtensionPropertiesFn
#define vkEnumerateDeviceLayerProperties \
  gpu::GetVulkanFunctionPointers()->vkEnumerateDeviceLayerPropertiesFn
#define vkEnumeratePhysicalDevices \
  gpu::GetVulkanFunctionPointers()->vkEnumeratePhysicalDevicesFn
#define vkGetDeviceProcAddr \
  gpu::GetVulkanFunctionPointers()->vkGetDeviceProcAddrFn
#define vkGetPhysicalDeviceFeatures \
  gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceFeaturesFn
#define vkGetPhysicalDeviceFormatProperties \
  gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceFormatPropertiesFn
#define vkGetPhysicalDeviceMemoryProperties \
  gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceMemoryPropertiesFn
#define vkGetPhysicalDeviceProperties \
  gpu::GetVulkanFunctionPointers()->vkGetPhysicalDevicePropertiesFn
#define vkGetPhysicalDeviceQueueFamilyProperties \
  gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceQueueFamilyPropertiesFn

#if DCHECK_IS_ON()
#define vkCreateDebugReportCallbackEXT \
  gpu::GetVulkanFunctionPointers()->vkCreateDebugReportCallbackEXTFn
#define vkDestroyDebugReportCallbackEXT \
  gpu::GetVulkanFunctionPointers()->vkDestroyDebugReportCallbackEXTFn
#endif  // DCHECK_IS_ON()

#define vkDestroySurfaceKHR \
  gpu::GetVulkanFunctionPointers()->vkDestroySurfaceKHRFn
#define vkGetPhysicalDeviceSurfaceCapabilitiesKHR \
  gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn
#define vkGetPhysicalDeviceSurfaceFormatsKHR \
  gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceSurfaceFormatsKHRFn
#define vkGetPhysicalDeviceSurfaceSupportKHR \
  gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceSurfaceSupportKHRFn

#if defined(USE_VULKAN_XLIB)
#define vkCreateXlibSurfaceKHR \
  gpu::GetVulkanFunctionPointers()->vkCreateXlibSurfaceKHRFn
#define vkGetPhysicalDeviceXlibPresentationSupportKHR \
  gpu::GetVulkanFunctionPointers()                    \
      ->vkGetPhysicalDeviceXlibPresentationSupportKHRFn
#endif  // defined(USE_VULKAN_XLIB)

#if defined(OS_ANDROID)
#define vkCreateAndroidSurfaceKHR \
  gpu::GetVulkanFunctionPointers()->vkCreateAndroidSurfaceKHRFn
#endif  // defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
#define vkCreateImagePipeSurfaceFUCHSIA \
  gpu::GetVulkanFunctionPointers()->vkCreateImagePipeSurfaceFUCHSIAFn
#endif  // defined(OS_FUCHSIA)

#define vkGetPhysicalDeviceImageFormatProperties2 \
  gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceImageFormatProperties2Fn

#define vkGetPhysicalDeviceFeatures2 \
  gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceFeatures2Fn

// Device functions
#define vkAllocateCommandBuffers \
  gpu::GetVulkanFunctionPointers()->vkAllocateCommandBuffersFn
#define vkAllocateDescriptorSets \
  gpu::GetVulkanFunctionPointers()->vkAllocateDescriptorSetsFn
#define vkAllocateMemory gpu::GetVulkanFunctionPointers()->vkAllocateMemoryFn
#define vkBeginCommandBuffer \
  gpu::GetVulkanFunctionPointers()->vkBeginCommandBufferFn
#define vkBindBufferMemory \
  gpu::GetVulkanFunctionPointers()->vkBindBufferMemoryFn
#define vkBindImageMemory gpu::GetVulkanFunctionPointers()->vkBindImageMemoryFn
#define vkCmdBeginRenderPass \
  gpu::GetVulkanFunctionPointers()->vkCmdBeginRenderPassFn
#define vkCmdCopyBufferToImage \
  gpu::GetVulkanFunctionPointers()->vkCmdCopyBufferToImageFn
#define vkCmdEndRenderPass \
  gpu::GetVulkanFunctionPointers()->vkCmdEndRenderPassFn
#define vkCmdExecuteCommands \
  gpu::GetVulkanFunctionPointers()->vkCmdExecuteCommandsFn
#define vkCmdNextSubpass gpu::GetVulkanFunctionPointers()->vkCmdNextSubpassFn
#define vkCmdPipelineBarrier \
  gpu::GetVulkanFunctionPointers()->vkCmdPipelineBarrierFn
#define vkCreateBuffer gpu::GetVulkanFunctionPointers()->vkCreateBufferFn
#define vkCreateCommandPool \
  gpu::GetVulkanFunctionPointers()->vkCreateCommandPoolFn
#define vkCreateDescriptorPool \
  gpu::GetVulkanFunctionPointers()->vkCreateDescriptorPoolFn
#define vkCreateDescriptorSetLayout \
  gpu::GetVulkanFunctionPointers()->vkCreateDescriptorSetLayoutFn
#define vkCreateFence gpu::GetVulkanFunctionPointers()->vkCreateFenceFn
#define vkCreateFramebuffer \
  gpu::GetVulkanFunctionPointers()->vkCreateFramebufferFn
#define vkCreateImage gpu::GetVulkanFunctionPointers()->vkCreateImageFn
#define vkCreateImageView gpu::GetVulkanFunctionPointers()->vkCreateImageViewFn
#define vkCreateRenderPass \
  gpu::GetVulkanFunctionPointers()->vkCreateRenderPassFn
#define vkCreateSampler gpu::GetVulkanFunctionPointers()->vkCreateSamplerFn
#define vkCreateSemaphore gpu::GetVulkanFunctionPointers()->vkCreateSemaphoreFn
#define vkCreateShaderModule \
  gpu::GetVulkanFunctionPointers()->vkCreateShaderModuleFn
#define vkDestroyBuffer gpu::GetVulkanFunctionPointers()->vkDestroyBufferFn
#define vkDestroyCommandPool \
  gpu::GetVulkanFunctionPointers()->vkDestroyCommandPoolFn
#define vkDestroyDescriptorPool \
  gpu::GetVulkanFunctionPointers()->vkDestroyDescriptorPoolFn
#define vkDestroyDescriptorSetLayout \
  gpu::GetVulkanFunctionPointers()->vkDestroyDescriptorSetLayoutFn
#define vkDestroyDevice gpu::GetVulkanFunctionPointers()->vkDestroyDeviceFn
#define vkDestroyFence gpu::GetVulkanFunctionPointers()->vkDestroyFenceFn
#define vkDestroyFramebuffer \
  gpu::GetVulkanFunctionPointers()->vkDestroyFramebufferFn
#define vkDestroyImage gpu::GetVulkanFunctionPointers()->vkDestroyImageFn
#define vkDestroyImageView \
  gpu::GetVulkanFunctionPointers()->vkDestroyImageViewFn
#define vkDestroyRenderPass \
  gpu::GetVulkanFunctionPointers()->vkDestroyRenderPassFn
#define vkDestroySampler gpu::GetVulkanFunctionPointers()->vkDestroySamplerFn
#define vkDestroySemaphore \
  gpu::GetVulkanFunctionPointers()->vkDestroySemaphoreFn
#define vkDestroyShaderModule \
  gpu::GetVulkanFunctionPointers()->vkDestroyShaderModuleFn
#define vkDeviceWaitIdle gpu::GetVulkanFunctionPointers()->vkDeviceWaitIdleFn
#define vkEndCommandBuffer \
  gpu::GetVulkanFunctionPointers()->vkEndCommandBufferFn
#define vkFreeCommandBuffers \
  gpu::GetVulkanFunctionPointers()->vkFreeCommandBuffersFn
#define vkFreeDescriptorSets \
  gpu::GetVulkanFunctionPointers()->vkFreeDescriptorSetsFn
#define vkFreeMemory gpu::GetVulkanFunctionPointers()->vkFreeMemoryFn
#define vkGetBufferMemoryRequirements \
  gpu::GetVulkanFunctionPointers()->vkGetBufferMemoryRequirementsFn
#define vkGetDeviceQueue gpu::GetVulkanFunctionPointers()->vkGetDeviceQueueFn
#define vkGetFenceStatus gpu::GetVulkanFunctionPointers()->vkGetFenceStatusFn
#define vkGetImageMemoryRequirements \
  gpu::GetVulkanFunctionPointers()->vkGetImageMemoryRequirementsFn
#define vkMapMemory gpu::GetVulkanFunctionPointers()->vkMapMemoryFn
#define vkQueueSubmit gpu::GetVulkanFunctionPointers()->vkQueueSubmitFn
#define vkQueueWaitIdle gpu::GetVulkanFunctionPointers()->vkQueueWaitIdleFn
#define vkResetCommandBuffer \
  gpu::GetVulkanFunctionPointers()->vkResetCommandBufferFn
#define vkResetFences gpu::GetVulkanFunctionPointers()->vkResetFencesFn
#define vkUnmapMemory gpu::GetVulkanFunctionPointers()->vkUnmapMemoryFn
#define vkUpdateDescriptorSets \
  gpu::GetVulkanFunctionPointers()->vkUpdateDescriptorSetsFn
#define vkWaitForFences gpu::GetVulkanFunctionPointers()->vkWaitForFencesFn

#define vkGetDeviceQueue2 gpu::GetVulkanFunctionPointers()->vkGetDeviceQueue2Fn
#define vkGetImageMemoryRequirements2 \
  gpu::GetVulkanFunctionPointers()->vkGetImageMemoryRequirements2Fn

#if defined(OS_ANDROID)
#define vkGetAndroidHardwareBufferPropertiesANDROID \
  gpu::GetVulkanFunctionPointers()                  \
      ->vkGetAndroidHardwareBufferPropertiesANDROIDFn
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
#define vkGetSemaphoreFdKHR \
  gpu::GetVulkanFunctionPointers()->vkGetSemaphoreFdKHRFn
#define vkImportSemaphoreFdKHR \
  gpu::GetVulkanFunctionPointers()->vkImportSemaphoreFdKHRFn
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
#define vkGetMemoryFdKHR gpu::GetVulkanFunctionPointers()->vkGetMemoryFdKHRFn
#define vkGetMemoryFdPropertiesKHR \
  gpu::GetVulkanFunctionPointers()->vkGetMemoryFdPropertiesKHRFn
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
#define vkImportSemaphoreZirconHandleFUCHSIA \
  gpu::GetVulkanFunctionPointers()->vkImportSemaphoreZirconHandleFUCHSIAFn
#define vkGetSemaphoreZirconHandleFUCHSIA \
  gpu::GetVulkanFunctionPointers()->vkGetSemaphoreZirconHandleFUCHSIAFn
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
#define vkGetMemoryZirconHandleFUCHSIA \
  gpu::GetVulkanFunctionPointers()->vkGetMemoryZirconHandleFUCHSIAFn
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
#define vkCreateBufferCollectionFUCHSIA \
  gpu::GetVulkanFunctionPointers()->vkCreateBufferCollectionFUCHSIAFn
#define vkSetBufferCollectionConstraintsFUCHSIA \
  gpu::GetVulkanFunctionPointers()->vkSetBufferCollectionConstraintsFUCHSIAFn
#define vkGetBufferCollectionPropertiesFUCHSIA \
  gpu::GetVulkanFunctionPointers()->vkGetBufferCollectionPropertiesFUCHSIAFn
#define vkDestroyBufferCollectionFUCHSIA \
  gpu::GetVulkanFunctionPointers()->vkDestroyBufferCollectionFUCHSIAFn
#endif  // defined(OS_FUCHSIA)

#define vkAcquireNextImageKHR \
  gpu::GetVulkanFunctionPointers()->vkAcquireNextImageKHRFn
#define vkCreateSwapchainKHR \
  gpu::GetVulkanFunctionPointers()->vkCreateSwapchainKHRFn
#define vkDestroySwapchainKHR \
  gpu::GetVulkanFunctionPointers()->vkDestroySwapchainKHRFn
#define vkGetSwapchainImagesKHR \
  gpu::GetVulkanFunctionPointers()->vkGetSwapchainImagesKHRFn
#define vkQueuePresentKHR gpu::GetVulkanFunctionPointers()->vkQueuePresentKHRFn

#endif  // GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_