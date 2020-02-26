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
  // Unassociated functions

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetInstanceProcAddrFn(Types... args)
      -> decltype(vkGetInstanceProcAddrFn_(args...)) {
    return vkGetInstanceProcAddrFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkEnumerateInstanceVersionFn(Types... args)
      -> decltype(vkEnumerateInstanceVersionFn_(args...)) {
    return vkEnumerateInstanceVersionFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateInstanceFn(Types... args)
      -> decltype(vkCreateInstanceFn_(args...)) {
    return vkCreateInstanceFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkEnumerateInstanceExtensionPropertiesFn(Types... args)
      -> decltype(vkEnumerateInstanceExtensionPropertiesFn_(args...)) {
    return vkEnumerateInstanceExtensionPropertiesFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkEnumerateInstanceLayerPropertiesFn(Types... args)
      -> decltype(vkEnumerateInstanceLayerPropertiesFn_(args...)) {
    return vkEnumerateInstanceLayerPropertiesFn_(args...);
  }

  // Instance functions

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateDeviceFn(Types... args) -> decltype(vkCreateDeviceFn_(args...)) {
    return vkCreateDeviceFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyInstanceFn(Types... args)
      -> decltype(vkDestroyInstanceFn_(args...)) {
    return vkDestroyInstanceFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkEnumerateDeviceExtensionPropertiesFn(Types... args)
      -> decltype(vkEnumerateDeviceExtensionPropertiesFn_(args...)) {
    return vkEnumerateDeviceExtensionPropertiesFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkEnumerateDeviceLayerPropertiesFn(Types... args)
      -> decltype(vkEnumerateDeviceLayerPropertiesFn_(args...)) {
    return vkEnumerateDeviceLayerPropertiesFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkEnumeratePhysicalDevicesFn(Types... args)
      -> decltype(vkEnumeratePhysicalDevicesFn_(args...)) {
    return vkEnumeratePhysicalDevicesFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetDeviceProcAddrFn(Types... args)
      -> decltype(vkGetDeviceProcAddrFn_(args...)) {
    return vkGetDeviceProcAddrFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDeviceFeaturesFn(Types... args)
      -> decltype(vkGetPhysicalDeviceFeaturesFn_(args...)) {
    return vkGetPhysicalDeviceFeaturesFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDeviceFormatPropertiesFn(Types... args)
      -> decltype(vkGetPhysicalDeviceFormatPropertiesFn_(args...)) {
    return vkGetPhysicalDeviceFormatPropertiesFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDeviceMemoryPropertiesFn(Types... args)
      -> decltype(vkGetPhysicalDeviceMemoryPropertiesFn_(args...)) {
    return vkGetPhysicalDeviceMemoryPropertiesFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDevicePropertiesFn(Types... args)
      -> decltype(vkGetPhysicalDevicePropertiesFn_(args...)) {
    return vkGetPhysicalDevicePropertiesFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDeviceQueueFamilyPropertiesFn(Types... args)
      -> decltype(vkGetPhysicalDeviceQueueFamilyPropertiesFn_(args...)) {
    return vkGetPhysicalDeviceQueueFamilyPropertiesFn_(args...);
  }

#if DCHECK_IS_ON()

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateDebugReportCallbackEXTFn(Types... args)
      -> decltype(vkCreateDebugReportCallbackEXTFn_(args...)) {
    return vkCreateDebugReportCallbackEXTFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyDebugReportCallbackEXTFn(Types... args)
      -> decltype(vkDestroyDebugReportCallbackEXTFn_(args...)) {
    return vkDestroyDebugReportCallbackEXTFn_(args...);
  }
#endif  // DCHECK_IS_ON()

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroySurfaceKHRFn(Types... args)
      -> decltype(vkDestroySurfaceKHRFn_(args...)) {
    return vkDestroySurfaceKHRFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn(Types... args)
      -> decltype(vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn_(args...)) {
    return vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDeviceSurfaceFormatsKHRFn(Types... args)
      -> decltype(vkGetPhysicalDeviceSurfaceFormatsKHRFn_(args...)) {
    return vkGetPhysicalDeviceSurfaceFormatsKHRFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDeviceSurfaceSupportKHRFn(Types... args)
      -> decltype(vkGetPhysicalDeviceSurfaceSupportKHRFn_(args...)) {
    return vkGetPhysicalDeviceSurfaceSupportKHRFn_(args...);
  }

#if defined(USE_VULKAN_XLIB)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateXlibSurfaceKHRFn(Types... args)
      -> decltype(vkCreateXlibSurfaceKHRFn_(args...)) {
    return vkCreateXlibSurfaceKHRFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDeviceXlibPresentationSupportKHRFn(Types... args)
      -> decltype(vkGetPhysicalDeviceXlibPresentationSupportKHRFn_(args...)) {
    return vkGetPhysicalDeviceXlibPresentationSupportKHRFn_(args...);
  }
#endif  // defined(USE_VULKAN_XLIB)

#if defined(OS_ANDROID)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateAndroidSurfaceKHRFn(Types... args)
      -> decltype(vkCreateAndroidSurfaceKHRFn_(args...)) {
    return vkCreateAndroidSurfaceKHRFn_(args...);
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_FUCHSIA)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateImagePipeSurfaceFUCHSIAFn(Types... args)
      -> decltype(vkCreateImagePipeSurfaceFUCHSIAFn_(args...)) {
    return vkCreateImagePipeSurfaceFUCHSIAFn_(args...);
  }
#endif  // defined(OS_FUCHSIA)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDeviceImageFormatProperties2Fn(Types... args)
      -> decltype(vkGetPhysicalDeviceImageFormatProperties2Fn_(args...)) {
    return vkGetPhysicalDeviceImageFormatProperties2Fn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetPhysicalDeviceFeatures2Fn(Types... args)
      -> decltype(vkGetPhysicalDeviceFeatures2Fn_(args...)) {
    return vkGetPhysicalDeviceFeatures2Fn_(args...);
  }

  // Device functions

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkAllocateCommandBuffersFn(Types... args)
      -> decltype(vkAllocateCommandBuffersFn_(args...)) {
    return vkAllocateCommandBuffersFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkAllocateDescriptorSetsFn(Types... args)
      -> decltype(vkAllocateDescriptorSetsFn_(args...)) {
    return vkAllocateDescriptorSetsFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkAllocateMemoryFn(Types... args)
      -> decltype(vkAllocateMemoryFn_(args...)) {
    return vkAllocateMemoryFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkBeginCommandBufferFn(Types... args)
      -> decltype(vkBeginCommandBufferFn_(args...)) {
    return vkBeginCommandBufferFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkBindBufferMemoryFn(Types... args)
      -> decltype(vkBindBufferMemoryFn_(args...)) {
    return vkBindBufferMemoryFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkBindImageMemoryFn(Types... args)
      -> decltype(vkBindImageMemoryFn_(args...)) {
    return vkBindImageMemoryFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCmdBeginRenderPassFn(Types... args)
      -> decltype(vkCmdBeginRenderPassFn_(args...)) {
    return vkCmdBeginRenderPassFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCmdCopyBufferToImageFn(Types... args)
      -> decltype(vkCmdCopyBufferToImageFn_(args...)) {
    return vkCmdCopyBufferToImageFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCmdEndRenderPassFn(Types... args)
      -> decltype(vkCmdEndRenderPassFn_(args...)) {
    return vkCmdEndRenderPassFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCmdExecuteCommandsFn(Types... args)
      -> decltype(vkCmdExecuteCommandsFn_(args...)) {
    return vkCmdExecuteCommandsFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCmdNextSubpassFn(Types... args)
      -> decltype(vkCmdNextSubpassFn_(args...)) {
    return vkCmdNextSubpassFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCmdPipelineBarrierFn(Types... args)
      -> decltype(vkCmdPipelineBarrierFn_(args...)) {
    return vkCmdPipelineBarrierFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateBufferFn(Types... args) -> decltype(vkCreateBufferFn_(args...)) {
    return vkCreateBufferFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateCommandPoolFn(Types... args)
      -> decltype(vkCreateCommandPoolFn_(args...)) {
    return vkCreateCommandPoolFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateDescriptorPoolFn(Types... args)
      -> decltype(vkCreateDescriptorPoolFn_(args...)) {
    return vkCreateDescriptorPoolFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateDescriptorSetLayoutFn(Types... args)
      -> decltype(vkCreateDescriptorSetLayoutFn_(args...)) {
    return vkCreateDescriptorSetLayoutFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateFenceFn(Types... args) -> decltype(vkCreateFenceFn_(args...)) {
    return vkCreateFenceFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateFramebufferFn(Types... args)
      -> decltype(vkCreateFramebufferFn_(args...)) {
    return vkCreateFramebufferFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateImageFn(Types... args) -> decltype(vkCreateImageFn_(args...)) {
    return vkCreateImageFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateImageViewFn(Types... args)
      -> decltype(vkCreateImageViewFn_(args...)) {
    return vkCreateImageViewFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateRenderPassFn(Types... args)
      -> decltype(vkCreateRenderPassFn_(args...)) {
    return vkCreateRenderPassFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateSamplerFn(Types... args)
      -> decltype(vkCreateSamplerFn_(args...)) {
    return vkCreateSamplerFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateSemaphoreFn(Types... args)
      -> decltype(vkCreateSemaphoreFn_(args...)) {
    return vkCreateSemaphoreFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateShaderModuleFn(Types... args)
      -> decltype(vkCreateShaderModuleFn_(args...)) {
    return vkCreateShaderModuleFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyBufferFn(Types... args)
      -> decltype(vkDestroyBufferFn_(args...)) {
    return vkDestroyBufferFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyCommandPoolFn(Types... args)
      -> decltype(vkDestroyCommandPoolFn_(args...)) {
    return vkDestroyCommandPoolFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyDescriptorPoolFn(Types... args)
      -> decltype(vkDestroyDescriptorPoolFn_(args...)) {
    return vkDestroyDescriptorPoolFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyDescriptorSetLayoutFn(Types... args)
      -> decltype(vkDestroyDescriptorSetLayoutFn_(args...)) {
    return vkDestroyDescriptorSetLayoutFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyDeviceFn(Types... args)
      -> decltype(vkDestroyDeviceFn_(args...)) {
    return vkDestroyDeviceFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyFenceFn(Types... args) -> decltype(vkDestroyFenceFn_(args...)) {
    return vkDestroyFenceFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyFramebufferFn(Types... args)
      -> decltype(vkDestroyFramebufferFn_(args...)) {
    return vkDestroyFramebufferFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyImageFn(Types... args) -> decltype(vkDestroyImageFn_(args...)) {
    return vkDestroyImageFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyImageViewFn(Types... args)
      -> decltype(vkDestroyImageViewFn_(args...)) {
    return vkDestroyImageViewFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyRenderPassFn(Types... args)
      -> decltype(vkDestroyRenderPassFn_(args...)) {
    return vkDestroyRenderPassFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroySamplerFn(Types... args)
      -> decltype(vkDestroySamplerFn_(args...)) {
    return vkDestroySamplerFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroySemaphoreFn(Types... args)
      -> decltype(vkDestroySemaphoreFn_(args...)) {
    return vkDestroySemaphoreFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyShaderModuleFn(Types... args)
      -> decltype(vkDestroyShaderModuleFn_(args...)) {
    return vkDestroyShaderModuleFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDeviceWaitIdleFn(Types... args)
      -> decltype(vkDeviceWaitIdleFn_(args...)) {
    return vkDeviceWaitIdleFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkEndCommandBufferFn(Types... args)
      -> decltype(vkEndCommandBufferFn_(args...)) {
    return vkEndCommandBufferFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkFreeCommandBuffersFn(Types... args)
      -> decltype(vkFreeCommandBuffersFn_(args...)) {
    return vkFreeCommandBuffersFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkFreeDescriptorSetsFn(Types... args)
      -> decltype(vkFreeDescriptorSetsFn_(args...)) {
    return vkFreeDescriptorSetsFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkFreeMemoryFn(Types... args) -> decltype(vkFreeMemoryFn_(args...)) {
    return vkFreeMemoryFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetBufferMemoryRequirementsFn(Types... args)
      -> decltype(vkGetBufferMemoryRequirementsFn_(args...)) {
    return vkGetBufferMemoryRequirementsFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetDeviceQueueFn(Types... args)
      -> decltype(vkGetDeviceQueueFn_(args...)) {
    return vkGetDeviceQueueFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetFenceStatusFn(Types... args)
      -> decltype(vkGetFenceStatusFn_(args...)) {
    return vkGetFenceStatusFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetImageMemoryRequirementsFn(Types... args)
      -> decltype(vkGetImageMemoryRequirementsFn_(args...)) {
    return vkGetImageMemoryRequirementsFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkMapMemoryFn(Types... args) -> decltype(vkMapMemoryFn_(args...)) {
    return vkMapMemoryFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkQueueSubmitFn(Types... args) -> decltype(vkQueueSubmitFn_(args...)) {
    return vkQueueSubmitFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkQueueWaitIdleFn(Types... args)
      -> decltype(vkQueueWaitIdleFn_(args...)) {
    return vkQueueWaitIdleFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkResetCommandBufferFn(Types... args)
      -> decltype(vkResetCommandBufferFn_(args...)) {
    return vkResetCommandBufferFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkResetFencesFn(Types... args) -> decltype(vkResetFencesFn_(args...)) {
    return vkResetFencesFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkUnmapMemoryFn(Types... args) -> decltype(vkUnmapMemoryFn_(args...)) {
    return vkUnmapMemoryFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkUpdateDescriptorSetsFn(Types... args)
      -> decltype(vkUpdateDescriptorSetsFn_(args...)) {
    return vkUpdateDescriptorSetsFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkWaitForFencesFn(Types... args)
      -> decltype(vkWaitForFencesFn_(args...)) {
    return vkWaitForFencesFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetDeviceQueue2Fn(Types... args)
      -> decltype(vkGetDeviceQueue2Fn_(args...)) {
    return vkGetDeviceQueue2Fn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetImageMemoryRequirements2Fn(Types... args)
      -> decltype(vkGetImageMemoryRequirements2Fn_(args...)) {
    return vkGetImageMemoryRequirements2Fn_(args...);
  }

#if defined(OS_ANDROID)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetAndroidHardwareBufferPropertiesANDROIDFn(Types... args)
      -> decltype(vkGetAndroidHardwareBufferPropertiesANDROIDFn_(args...)) {
    return vkGetAndroidHardwareBufferPropertiesANDROIDFn_(args...);
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetSemaphoreFdKHRFn(Types... args)
      -> decltype(vkGetSemaphoreFdKHRFn_(args...)) {
    return vkGetSemaphoreFdKHRFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkImportSemaphoreFdKHRFn(Types... args)
      -> decltype(vkImportSemaphoreFdKHRFn_(args...)) {
    return vkImportSemaphoreFdKHRFn_(args...);
  }
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetMemoryFdKHRFn(Types... args)
      -> decltype(vkGetMemoryFdKHRFn_(args...)) {
    return vkGetMemoryFdKHRFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetMemoryFdPropertiesKHRFn(Types... args)
      -> decltype(vkGetMemoryFdPropertiesKHRFn_(args...)) {
    return vkGetMemoryFdPropertiesKHRFn_(args...);
  }
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_FUCHSIA)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkImportSemaphoreZirconHandleFUCHSIAFn(Types... args)
      -> decltype(vkImportSemaphoreZirconHandleFUCHSIAFn_(args...)) {
    return vkImportSemaphoreZirconHandleFUCHSIAFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetSemaphoreZirconHandleFUCHSIAFn(Types... args)
      -> decltype(vkGetSemaphoreZirconHandleFUCHSIAFn_(args...)) {
    return vkGetSemaphoreZirconHandleFUCHSIAFn_(args...);
  }
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetMemoryZirconHandleFUCHSIAFn(Types... args)
      -> decltype(vkGetMemoryZirconHandleFUCHSIAFn_(args...)) {
    return vkGetMemoryZirconHandleFUCHSIAFn_(args...);
  }
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateBufferCollectionFUCHSIAFn(Types... args)
      -> decltype(vkCreateBufferCollectionFUCHSIAFn_(args...)) {
    return vkCreateBufferCollectionFUCHSIAFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkSetBufferCollectionConstraintsFUCHSIAFn(Types... args)
      -> decltype(vkSetBufferCollectionConstraintsFUCHSIAFn_(args...)) {
    return vkSetBufferCollectionConstraintsFUCHSIAFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetBufferCollectionPropertiesFUCHSIAFn(Types... args)
      -> decltype(vkGetBufferCollectionPropertiesFUCHSIAFn_(args...)) {
    return vkGetBufferCollectionPropertiesFUCHSIAFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroyBufferCollectionFUCHSIAFn(Types... args)
      -> decltype(vkDestroyBufferCollectionFUCHSIAFn_(args...)) {
    return vkDestroyBufferCollectionFUCHSIAFn_(args...);
  }
#endif  // defined(OS_FUCHSIA)

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkAcquireNextImageKHRFn(Types... args)
      -> decltype(vkAcquireNextImageKHRFn_(args...)) {
    return vkAcquireNextImageKHRFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkCreateSwapchainKHRFn(Types... args)
      -> decltype(vkCreateSwapchainKHRFn_(args...)) {
    return vkCreateSwapchainKHRFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkDestroySwapchainKHRFn(Types... args)
      -> decltype(vkDestroySwapchainKHRFn_(args...)) {
    return vkDestroySwapchainKHRFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkGetSwapchainImagesKHRFn(Types... args)
      -> decltype(vkGetSwapchainImagesKHRFn_(args...)) {
    return vkGetSwapchainImagesKHRFn_(args...);
  }

  template <typename... Types>
  NO_SANITIZE("cfi-icall")
  auto vkQueuePresentKHRFn(Types... args)
      -> decltype(vkQueuePresentKHRFn_(args...)) {
    return vkQueuePresentKHRFn_(args...);
  }
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
