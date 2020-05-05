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
#include "base/component_export.h"
#include "base/native_library.h"
#include "build/build_config.h"
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

#if defined(OS_WIN)
#include <vulkan/vulkan_win32.h>
#endif

namespace gpu {

struct VulkanFunctionPointers;

COMPONENT_EXPORT(VULKAN) VulkanFunctionPointers* GetVulkanFunctionPointers();

struct VulkanFunctionPointers {
  VulkanFunctionPointers();
  ~VulkanFunctionPointers();

  COMPONENT_EXPORT(VULKAN) bool BindUnassociatedFunctionPointers();

  // These functions assume that vkGetInstanceProcAddr has been populated.
  COMPONENT_EXPORT(VULKAN)
  bool BindInstanceFunctionPointers(
      VkInstance vk_instance,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  // These functions assume that vkGetDeviceProcAddr has been populated.
  COMPONENT_EXPORT(VULKAN)
  bool BindDeviceFunctionPointers(VkDevice vk_device,
                                  uint32_t api_version,
                                  const gfx::ExtensionSet& enabled_extensions);

  base::NativeLibrary vulkan_loader_library = nullptr;

  template <typename T>
  class VulkanFunction;
  template <typename R, typename... Args>
  class VulkanFunction<R(VKAPI_PTR*)(Args...)> {
   public:
    using Fn = R(VKAPI_PTR*)(Args...);

    explicit operator bool() { return !!fn_; }

    NO_SANITIZE("cfi-icall")
    R operator()(Args... args) { return fn_(args...); }

    Fn get() const { return fn_; }

   private:
    friend VulkanFunctionPointers;

    Fn operator=(Fn fn) {
      fn_ = fn;
      return fn_;
    }

    Fn fn_ = nullptr;
  };

  // Unassociated functions
  VulkanFunction<PFN_vkEnumerateInstanceVersion> vkEnumerateInstanceVersionFn;
  VulkanFunction<PFN_vkGetInstanceProcAddr> vkGetInstanceProcAddrFn;

  VulkanFunction<PFN_vkCreateInstance> vkCreateInstanceFn;
  VulkanFunction<PFN_vkEnumerateInstanceExtensionProperties>
      vkEnumerateInstanceExtensionPropertiesFn;
  VulkanFunction<PFN_vkEnumerateInstanceLayerProperties>
      vkEnumerateInstanceLayerPropertiesFn;

  // Instance functions
  VulkanFunction<PFN_vkCreateDevice> vkCreateDeviceFn;
  VulkanFunction<PFN_vkDestroyInstance> vkDestroyInstanceFn;
  VulkanFunction<PFN_vkEnumerateDeviceExtensionProperties>
      vkEnumerateDeviceExtensionPropertiesFn;
  VulkanFunction<PFN_vkEnumerateDeviceLayerProperties>
      vkEnumerateDeviceLayerPropertiesFn;
  VulkanFunction<PFN_vkEnumeratePhysicalDevices> vkEnumeratePhysicalDevicesFn;
  VulkanFunction<PFN_vkGetDeviceProcAddr> vkGetDeviceProcAddrFn;
  VulkanFunction<PFN_vkGetPhysicalDeviceFeatures> vkGetPhysicalDeviceFeaturesFn;
  VulkanFunction<PFN_vkGetPhysicalDeviceFormatProperties>
      vkGetPhysicalDeviceFormatPropertiesFn;
  VulkanFunction<PFN_vkGetPhysicalDeviceMemoryProperties>
      vkGetPhysicalDeviceMemoryPropertiesFn;
  VulkanFunction<PFN_vkGetPhysicalDeviceProperties>
      vkGetPhysicalDevicePropertiesFn;
  VulkanFunction<PFN_vkGetPhysicalDeviceQueueFamilyProperties>
      vkGetPhysicalDeviceQueueFamilyPropertiesFn;

#if DCHECK_IS_ON()
  VulkanFunction<PFN_vkCreateDebugReportCallbackEXT>
      vkCreateDebugReportCallbackEXTFn;
  VulkanFunction<PFN_vkDestroyDebugReportCallbackEXT>
      vkDestroyDebugReportCallbackEXTFn;
#endif  // DCHECK_IS_ON()

  VulkanFunction<PFN_vkDestroySurfaceKHR> vkDestroySurfaceKHRFn;
  VulkanFunction<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>
      vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn;
  VulkanFunction<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>
      vkGetPhysicalDeviceSurfaceFormatsKHRFn;
  VulkanFunction<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>
      vkGetPhysicalDeviceSurfaceSupportKHRFn;

#if defined(USE_VULKAN_XLIB)
  VulkanFunction<PFN_vkCreateXlibSurfaceKHR> vkCreateXlibSurfaceKHRFn;
  VulkanFunction<PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR>
      vkGetPhysicalDeviceXlibPresentationSupportKHRFn;
#endif  // defined(USE_VULKAN_XLIB)

#if defined(OS_WIN)
  VulkanFunction<PFN_vkCreateWin32SurfaceKHR> vkCreateWin32SurfaceKHRFn;
  VulkanFunction<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>
      vkGetPhysicalDeviceWin32PresentationSupportKHRFn;
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
  VulkanFunction<PFN_vkCreateAndroidSurfaceKHR> vkCreateAndroidSurfaceKHRFn;
#endif  // defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
  VulkanFunction<PFN_vkCreateImagePipeSurfaceFUCHSIA>
      vkCreateImagePipeSurfaceFUCHSIAFn;
#endif  // defined(OS_FUCHSIA)

  VulkanFunction<PFN_vkGetPhysicalDeviceImageFormatProperties2>
      vkGetPhysicalDeviceImageFormatProperties2Fn;

  VulkanFunction<PFN_vkGetPhysicalDeviceFeatures2>
      vkGetPhysicalDeviceFeatures2Fn;

  // Device functions
  VulkanFunction<PFN_vkAllocateCommandBuffers> vkAllocateCommandBuffersFn;
  VulkanFunction<PFN_vkAllocateDescriptorSets> vkAllocateDescriptorSetsFn;
  VulkanFunction<PFN_vkAllocateMemory> vkAllocateMemoryFn;
  VulkanFunction<PFN_vkBeginCommandBuffer> vkBeginCommandBufferFn;
  VulkanFunction<PFN_vkBindBufferMemory> vkBindBufferMemoryFn;
  VulkanFunction<PFN_vkBindImageMemory> vkBindImageMemoryFn;
  VulkanFunction<PFN_vkCmdBeginRenderPass> vkCmdBeginRenderPassFn;
  VulkanFunction<PFN_vkCmdCopyBuffer> vkCmdCopyBufferFn;
  VulkanFunction<PFN_vkCmdCopyBufferToImage> vkCmdCopyBufferToImageFn;
  VulkanFunction<PFN_vkCmdEndRenderPass> vkCmdEndRenderPassFn;
  VulkanFunction<PFN_vkCmdExecuteCommands> vkCmdExecuteCommandsFn;
  VulkanFunction<PFN_vkCmdNextSubpass> vkCmdNextSubpassFn;
  VulkanFunction<PFN_vkCmdPipelineBarrier> vkCmdPipelineBarrierFn;
  VulkanFunction<PFN_vkCreateBuffer> vkCreateBufferFn;
  VulkanFunction<PFN_vkCreateCommandPool> vkCreateCommandPoolFn;
  VulkanFunction<PFN_vkCreateDescriptorPool> vkCreateDescriptorPoolFn;
  VulkanFunction<PFN_vkCreateDescriptorSetLayout> vkCreateDescriptorSetLayoutFn;
  VulkanFunction<PFN_vkCreateFence> vkCreateFenceFn;
  VulkanFunction<PFN_vkCreateFramebuffer> vkCreateFramebufferFn;
  VulkanFunction<PFN_vkCreateImage> vkCreateImageFn;
  VulkanFunction<PFN_vkCreateImageView> vkCreateImageViewFn;
  VulkanFunction<PFN_vkCreateRenderPass> vkCreateRenderPassFn;
  VulkanFunction<PFN_vkCreateSampler> vkCreateSamplerFn;
  VulkanFunction<PFN_vkCreateSemaphore> vkCreateSemaphoreFn;
  VulkanFunction<PFN_vkCreateShaderModule> vkCreateShaderModuleFn;
  VulkanFunction<PFN_vkDestroyBuffer> vkDestroyBufferFn;
  VulkanFunction<PFN_vkDestroyCommandPool> vkDestroyCommandPoolFn;
  VulkanFunction<PFN_vkDestroyDescriptorPool> vkDestroyDescriptorPoolFn;
  VulkanFunction<PFN_vkDestroyDescriptorSetLayout>
      vkDestroyDescriptorSetLayoutFn;
  VulkanFunction<PFN_vkDestroyDevice> vkDestroyDeviceFn;
  VulkanFunction<PFN_vkDestroyFence> vkDestroyFenceFn;
  VulkanFunction<PFN_vkDestroyFramebuffer> vkDestroyFramebufferFn;
  VulkanFunction<PFN_vkDestroyImage> vkDestroyImageFn;
  VulkanFunction<PFN_vkDestroyImageView> vkDestroyImageViewFn;
  VulkanFunction<PFN_vkDestroyRenderPass> vkDestroyRenderPassFn;
  VulkanFunction<PFN_vkDestroySampler> vkDestroySamplerFn;
  VulkanFunction<PFN_vkDestroySemaphore> vkDestroySemaphoreFn;
  VulkanFunction<PFN_vkDestroyShaderModule> vkDestroyShaderModuleFn;
  VulkanFunction<PFN_vkDeviceWaitIdle> vkDeviceWaitIdleFn;
  VulkanFunction<PFN_vkFlushMappedMemoryRanges> vkFlushMappedMemoryRangesFn;
  VulkanFunction<PFN_vkEndCommandBuffer> vkEndCommandBufferFn;
  VulkanFunction<PFN_vkFreeCommandBuffers> vkFreeCommandBuffersFn;
  VulkanFunction<PFN_vkFreeDescriptorSets> vkFreeDescriptorSetsFn;
  VulkanFunction<PFN_vkFreeMemory> vkFreeMemoryFn;
  VulkanFunction<PFN_vkInvalidateMappedMemoryRanges>
      vkInvalidateMappedMemoryRangesFn;
  VulkanFunction<PFN_vkGetBufferMemoryRequirements>
      vkGetBufferMemoryRequirementsFn;
  VulkanFunction<PFN_vkGetDeviceQueue> vkGetDeviceQueueFn;
  VulkanFunction<PFN_vkGetFenceStatus> vkGetFenceStatusFn;
  VulkanFunction<PFN_vkGetImageMemoryRequirements>
      vkGetImageMemoryRequirementsFn;
  VulkanFunction<PFN_vkMapMemory> vkMapMemoryFn;
  VulkanFunction<PFN_vkQueueSubmit> vkQueueSubmitFn;
  VulkanFunction<PFN_vkQueueWaitIdle> vkQueueWaitIdleFn;
  VulkanFunction<PFN_vkResetCommandBuffer> vkResetCommandBufferFn;
  VulkanFunction<PFN_vkResetFences> vkResetFencesFn;
  VulkanFunction<PFN_vkUnmapMemory> vkUnmapMemoryFn;
  VulkanFunction<PFN_vkUpdateDescriptorSets> vkUpdateDescriptorSetsFn;
  VulkanFunction<PFN_vkWaitForFences> vkWaitForFencesFn;

  VulkanFunction<PFN_vkGetDeviceQueue2> vkGetDeviceQueue2Fn;
  VulkanFunction<PFN_vkGetBufferMemoryRequirements2>
      vkGetBufferMemoryRequirements2Fn;
  VulkanFunction<PFN_vkGetImageMemoryRequirements2>
      vkGetImageMemoryRequirements2Fn;

#if defined(OS_ANDROID)
  VulkanFunction<PFN_vkGetAndroidHardwareBufferPropertiesANDROID>
      vkGetAndroidHardwareBufferPropertiesANDROIDFn;
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  VulkanFunction<PFN_vkGetSemaphoreFdKHR> vkGetSemaphoreFdKHRFn;
  VulkanFunction<PFN_vkImportSemaphoreFdKHR> vkImportSemaphoreFdKHRFn;
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_WIN)
  VulkanFunction<PFN_vkGetSemaphoreWin32HandleKHR>
      vkGetSemaphoreWin32HandleKHRFn;
  VulkanFunction<PFN_vkImportSemaphoreWin32HandleKHR>
      vkImportSemaphoreWin32HandleKHRFn;
#endif  // defined(OS_WIN)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  VulkanFunction<PFN_vkGetMemoryFdKHR> vkGetMemoryFdKHRFn;
  VulkanFunction<PFN_vkGetMemoryFdPropertiesKHR> vkGetMemoryFdPropertiesKHRFn;
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_WIN)
  VulkanFunction<PFN_vkGetMemoryWin32HandleKHR> vkGetMemoryWin32HandleKHRFn;
  VulkanFunction<PFN_vkGetMemoryWin32HandlePropertiesKHR>
      vkGetMemoryWin32HandlePropertiesKHRFn;
#endif  // defined(OS_WIN)

#if defined(OS_FUCHSIA)
  VulkanFunction<PFN_vkImportSemaphoreZirconHandleFUCHSIA>
      vkImportSemaphoreZirconHandleFUCHSIAFn;
  VulkanFunction<PFN_vkGetSemaphoreZirconHandleFUCHSIA>
      vkGetSemaphoreZirconHandleFUCHSIAFn;
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  VulkanFunction<PFN_vkGetMemoryZirconHandleFUCHSIA>
      vkGetMemoryZirconHandleFUCHSIAFn;
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  VulkanFunction<PFN_vkCreateBufferCollectionFUCHSIA>
      vkCreateBufferCollectionFUCHSIAFn;
  VulkanFunction<PFN_vkSetBufferCollectionConstraintsFUCHSIA>
      vkSetBufferCollectionConstraintsFUCHSIAFn;
  VulkanFunction<PFN_vkGetBufferCollectionPropertiesFUCHSIA>
      vkGetBufferCollectionPropertiesFUCHSIAFn;
  VulkanFunction<PFN_vkDestroyBufferCollectionFUCHSIA>
      vkDestroyBufferCollectionFUCHSIAFn;
#endif  // defined(OS_FUCHSIA)

  VulkanFunction<PFN_vkAcquireNextImageKHR> vkAcquireNextImageKHRFn;
  VulkanFunction<PFN_vkCreateSwapchainKHR> vkCreateSwapchainKHRFn;
  VulkanFunction<PFN_vkDestroySwapchainKHR> vkDestroySwapchainKHRFn;
  VulkanFunction<PFN_vkGetSwapchainImagesKHR> vkGetSwapchainImagesKHRFn;
  VulkanFunction<PFN_vkQueuePresentKHR> vkQueuePresentKHRFn;
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

#if defined(OS_WIN)
#define vkCreateWin32SurfaceKHR \
  gpu::GetVulkanFunctionPointers()->vkCreateWin32SurfaceKHRFn
#define vkGetPhysicalDeviceWin32PresentationSupportKHR \
  gpu::GetVulkanFunctionPointers()                     \
      ->vkGetPhysicalDeviceWin32PresentationSupportKHRFn
#endif  // defined(OS_WIN)

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
#define vkCmdCopyBuffer gpu::GetVulkanFunctionPointers()->vkCmdCopyBufferFn
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
#define vkFlushMappedMemoryRanges \
  gpu::GetVulkanFunctionPointers()->vkFlushMappedMemoryRangesFn
#define vkEndCommandBuffer \
  gpu::GetVulkanFunctionPointers()->vkEndCommandBufferFn
#define vkFreeCommandBuffers \
  gpu::GetVulkanFunctionPointers()->vkFreeCommandBuffersFn
#define vkFreeDescriptorSets \
  gpu::GetVulkanFunctionPointers()->vkFreeDescriptorSetsFn
#define vkFreeMemory gpu::GetVulkanFunctionPointers()->vkFreeMemoryFn
#define vkInvalidateMappedMemoryRanges \
  gpu::GetVulkanFunctionPointers()->vkInvalidateMappedMemoryRangesFn
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
#define vkGetBufferMemoryRequirements2 \
  gpu::GetVulkanFunctionPointers()->vkGetBufferMemoryRequirements2Fn
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

#if defined(OS_WIN)
#define vkGetSemaphoreWin32HandleKHR \
  gpu::GetVulkanFunctionPointers()->vkGetSemaphoreWin32HandleKHRFn
#define vkImportSemaphoreWin32HandleKHR \
  gpu::GetVulkanFunctionPointers()->vkImportSemaphoreWin32HandleKHRFn
#endif  // defined(OS_WIN)

#if defined(OS_LINUX) || defined(OS_ANDROID)
#define vkGetMemoryFdKHR gpu::GetVulkanFunctionPointers()->vkGetMemoryFdKHRFn
#define vkGetMemoryFdPropertiesKHR \
  gpu::GetVulkanFunctionPointers()->vkGetMemoryFdPropertiesKHRFn
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_WIN)
#define vkGetMemoryWin32HandleKHR \
  gpu::GetVulkanFunctionPointers()->vkGetMemoryWin32HandleKHRFn
#define vkGetMemoryWin32HandlePropertiesKHR \
  gpu::GetVulkanFunctionPointers()->vkGetMemoryWin32HandlePropertiesKHRFn
#endif  // defined(OS_WIN)

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