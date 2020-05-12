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

struct COMPONENT_EXPORT(VULKAN) VulkanFunctionPointers {
  VulkanFunctionPointers();
  ~VulkanFunctionPointers();

  bool BindUnassociatedFunctionPointers();

  // These functions assume that vkGetInstanceProcAddr has been populated.
  bool BindInstanceFunctionPointers(
      VkInstance vk_instance,
      uint32_t api_version,
      const gfx::ExtensionSet& enabled_extensions);

  // These functions assume that vkGetDeviceProcAddr has been populated.
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
ALWAYS_INLINE PFN_vkVoidFunction vkGetInstanceProcAddr(VkInstance instance,
                                                       const char* pName) {
  return gpu::GetVulkanFunctionPointers()->vkGetInstanceProcAddrFn(instance,
                                                                   pName);
}
ALWAYS_INLINE VkResult vkEnumerateInstanceVersion(uint32_t* pApiVersion) {
  return gpu::GetVulkanFunctionPointers()->vkEnumerateInstanceVersionFn(
      pApiVersion);
}

ALWAYS_INLINE VkResult vkCreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                        const VkAllocationCallbacks* pAllocator,
                                        VkInstance* pInstance) {
  return gpu::GetVulkanFunctionPointers()->vkCreateInstanceFn(
      pCreateInfo, pAllocator, pInstance);
}
ALWAYS_INLINE VkResult
vkEnumerateInstanceExtensionProperties(const char* pLayerName,
                                       uint32_t* pPropertyCount,
                                       VkExtensionProperties* pProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkEnumerateInstanceExtensionPropertiesFn(pLayerName, pPropertyCount,
                                                 pProperties);
}
ALWAYS_INLINE VkResult
vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount,
                                   VkLayerProperties* pProperties) {
  return gpu::GetVulkanFunctionPointers()->vkEnumerateInstanceLayerPropertiesFn(
      pPropertyCount, pProperties);
}

// Instance functions
ALWAYS_INLINE VkResult vkCreateDevice(VkPhysicalDevice physicalDevice,
                                      const VkDeviceCreateInfo* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkDevice* pDevice) {
  return gpu::GetVulkanFunctionPointers()->vkCreateDeviceFn(
      physicalDevice, pCreateInfo, pAllocator, pDevice);
}
ALWAYS_INLINE void vkDestroyInstance(VkInstance instance,
                                     const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyInstanceFn(instance,
                                                               pAllocator);
}
ALWAYS_INLINE VkResult
vkEnumerateDeviceExtensionProperties(VkPhysicalDevice physicalDevice,
                                     const char* pLayerName,
                                     uint32_t* pPropertyCount,
                                     VkExtensionProperties* pProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkEnumerateDeviceExtensionPropertiesFn(physicalDevice, pLayerName,
                                               pPropertyCount, pProperties);
}
ALWAYS_INLINE VkResult
vkEnumerateDeviceLayerProperties(VkPhysicalDevice physicalDevice,
                                 uint32_t* pPropertyCount,
                                 VkLayerProperties* pProperties) {
  return gpu::GetVulkanFunctionPointers()->vkEnumerateDeviceLayerPropertiesFn(
      physicalDevice, pPropertyCount, pProperties);
}
ALWAYS_INLINE VkResult
vkEnumeratePhysicalDevices(VkInstance instance,
                           uint32_t* pPhysicalDeviceCount,
                           VkPhysicalDevice* pPhysicalDevices) {
  return gpu::GetVulkanFunctionPointers()->vkEnumeratePhysicalDevicesFn(
      instance, pPhysicalDeviceCount, pPhysicalDevices);
}
ALWAYS_INLINE PFN_vkVoidFunction vkGetDeviceProcAddr(VkDevice device,
                                                     const char* pName) {
  return gpu::GetVulkanFunctionPointers()->vkGetDeviceProcAddrFn(device, pName);
}
ALWAYS_INLINE void vkGetPhysicalDeviceFeatures(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceFeatures* pFeatures) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceFeaturesFn(
      physicalDevice, pFeatures);
}
ALWAYS_INLINE void vkGetPhysicalDeviceFormatProperties(
    VkPhysicalDevice physicalDevice,
    VkFormat format,
    VkFormatProperties* pFormatProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceFormatPropertiesFn(physicalDevice, format,
                                              pFormatProperties);
}
ALWAYS_INLINE void vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties* pMemoryProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceMemoryPropertiesFn(physicalDevice,
                                              pMemoryProperties);
}
ALWAYS_INLINE void vkGetPhysicalDeviceProperties(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceProperties* pProperties) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDevicePropertiesFn(
      physicalDevice, pProperties);
}
ALWAYS_INLINE void vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice physicalDevice,
    uint32_t* pQueueFamilyPropertyCount,
    VkQueueFamilyProperties* pQueueFamilyProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceQueueFamilyPropertiesFn(
          physicalDevice, pQueueFamilyPropertyCount, pQueueFamilyProperties);
}

#if DCHECK_IS_ON()
ALWAYS_INLINE VkResult vkCreateDebugReportCallbackEXT(
    VkInstance instance,
    const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugReportCallbackEXT* pCallback) {
  return gpu::GetVulkanFunctionPointers()->vkCreateDebugReportCallbackEXTFn(
      instance, pCreateInfo, pAllocator, pCallback);
}
ALWAYS_INLINE void vkDestroyDebugReportCallbackEXT(
    VkInstance instance,
    VkDebugReportCallbackEXT callback,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyDebugReportCallbackEXTFn(
      instance, callback, pAllocator);
}
#endif  // DCHECK_IS_ON()

ALWAYS_INLINE void vkDestroySurfaceKHR(
    VkInstance instance,
    VkSurfaceKHR surface,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroySurfaceKHRFn(
      instance, surface, pAllocator);
}
ALWAYS_INLINE VkResult vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    VkSurfaceCapabilitiesKHR* pSurfaceCapabilities) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn(physicalDevice, surface,
                                                    pSurfaceCapabilities);
}
ALWAYS_INLINE VkResult
vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice physicalDevice,
                                     VkSurfaceKHR surface,
                                     uint32_t* pSurfaceFormatCount,
                                     VkSurfaceFormatKHR* pSurfaceFormats) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceSurfaceFormatsKHRFn(
          physicalDevice, surface, pSurfaceFormatCount, pSurfaceFormats);
}
ALWAYS_INLINE VkResult
vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice physicalDevice,
                                     uint32_t queueFamilyIndex,
                                     VkSurfaceKHR surface,
                                     VkBool32* pSupported) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceSurfaceSupportKHRFn(physicalDevice, queueFamilyIndex,
                                               surface, pSupported);
}

#if defined(USE_VULKAN_XLIB)
ALWAYS_INLINE VkResult
vkCreateXlibSurfaceKHR(VkInstance instance,
                       const VkXlibSurfaceCreateInfoKHR* pCreateInfo,
                       const VkAllocationCallbacks* pAllocator,
                       VkSurfaceKHR* pSurface) {
  return gpu::GetVulkanFunctionPointers()->vkCreateXlibSurfaceKHRFn(
      instance, pCreateInfo, pAllocator, pSurface);
}
ALWAYS_INLINE VkBool32
vkGetPhysicalDeviceXlibPresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                              uint32_t queueFamilyIndex,
                                              Display* dpy,
                                              VisualID visualID) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceXlibPresentationSupportKHRFn(
          physicalDevice, queueFamilyIndex, dpy, visualID);
}
#endif  // defined(USE_VULKAN_XLIB)

#if defined(OS_WIN)
ALWAYS_INLINE VkResult
vkCreateWin32SurfaceKHR(VkInstance instance,
                        const VkWin32SurfaceCreateInfoKHR* pCreateInfo,
                        const VkAllocationCallbacks* pAllocator,
                        VkSurfaceKHR* pSurface) {
  return gpu::GetVulkanFunctionPointers()->vkCreateWin32SurfaceKHRFn(
      instance, pCreateInfo, pAllocator, pSurface);
}
ALWAYS_INLINE VkBool32
vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice physicalDevice,
                                               uint32_t queueFamilyIndex) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceWin32PresentationSupportKHRFn(physicalDevice,
                                                         queueFamilyIndex);
}
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
ALWAYS_INLINE VkResult
vkCreateAndroidSurfaceKHR(VkInstance instance,
                          const VkAndroidSurfaceCreateInfoKHR* pCreateInfo,
                          const VkAllocationCallbacks* pAllocator,
                          VkSurfaceKHR* pSurface) {
  return gpu::GetVulkanFunctionPointers()->vkCreateAndroidSurfaceKHRFn(
      instance, pCreateInfo, pAllocator, pSurface);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
ALWAYS_INLINE VkResult vkCreateImagePipeSurfaceFUCHSIA(
    VkInstance instance,
    const VkImagePipeSurfaceCreateInfoFUCHSIA* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSurfaceKHR* pSurface) {
  return gpu::GetVulkanFunctionPointers()->vkCreateImagePipeSurfaceFUCHSIAFn(
      instance, pCreateInfo, pAllocator, pSurface);
}
#endif  // defined(OS_FUCHSIA)

ALWAYS_INLINE VkResult vkGetPhysicalDeviceImageFormatProperties2(
    VkPhysicalDevice physicalDevice,
    const VkPhysicalDeviceImageFormatInfo2* pImageFormatInfo,
    VkImageFormatProperties2* pImageFormatProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetPhysicalDeviceImageFormatProperties2Fn(
          physicalDevice, pImageFormatInfo, pImageFormatProperties);
}

ALWAYS_INLINE void vkGetPhysicalDeviceFeatures2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceFeatures2* pFeatures) {
  return gpu::GetVulkanFunctionPointers()->vkGetPhysicalDeviceFeatures2Fn(
      physicalDevice, pFeatures);
}

// Device functions
ALWAYS_INLINE VkResult
vkAllocateCommandBuffers(VkDevice device,
                         const VkCommandBufferAllocateInfo* pAllocateInfo,
                         VkCommandBuffer* pCommandBuffers) {
  return gpu::GetVulkanFunctionPointers()->vkAllocateCommandBuffersFn(
      device, pAllocateInfo, pCommandBuffers);
}
ALWAYS_INLINE VkResult
vkAllocateDescriptorSets(VkDevice device,
                         const VkDescriptorSetAllocateInfo* pAllocateInfo,
                         VkDescriptorSet* pDescriptorSets) {
  return gpu::GetVulkanFunctionPointers()->vkAllocateDescriptorSetsFn(
      device, pAllocateInfo, pDescriptorSets);
}
ALWAYS_INLINE VkResult
vkAllocateMemory(VkDevice device,
                 const VkMemoryAllocateInfo* pAllocateInfo,
                 const VkAllocationCallbacks* pAllocator,
                 VkDeviceMemory* pMemory) {
  return gpu::GetVulkanFunctionPointers()->vkAllocateMemoryFn(
      device, pAllocateInfo, pAllocator, pMemory);
}
ALWAYS_INLINE VkResult
vkBeginCommandBuffer(VkCommandBuffer commandBuffer,
                     const VkCommandBufferBeginInfo* pBeginInfo) {
  return gpu::GetVulkanFunctionPointers()->vkBeginCommandBufferFn(commandBuffer,
                                                                  pBeginInfo);
}
ALWAYS_INLINE VkResult vkBindBufferMemory(VkDevice device,
                                          VkBuffer buffer,
                                          VkDeviceMemory memory,
                                          VkDeviceSize memoryOffset) {
  return gpu::GetVulkanFunctionPointers()->vkBindBufferMemoryFn(
      device, buffer, memory, memoryOffset);
}
ALWAYS_INLINE VkResult vkBindImageMemory(VkDevice device,
                                         VkImage image,
                                         VkDeviceMemory memory,
                                         VkDeviceSize memoryOffset) {
  return gpu::GetVulkanFunctionPointers()->vkBindImageMemoryFn(
      device, image, memory, memoryOffset);
}
ALWAYS_INLINE void vkCmdBeginRenderPass(
    VkCommandBuffer commandBuffer,
    const VkRenderPassBeginInfo* pRenderPassBegin,
    VkSubpassContents contents) {
  return gpu::GetVulkanFunctionPointers()->vkCmdBeginRenderPassFn(
      commandBuffer, pRenderPassBegin, contents);
}
ALWAYS_INLINE void vkCmdCopyBuffer(VkCommandBuffer commandBuffer,
                                   VkBuffer srcBuffer,
                                   VkBuffer dstBuffer,
                                   uint32_t regionCount,
                                   const VkBufferCopy* pRegions) {
  return gpu::GetVulkanFunctionPointers()->vkCmdCopyBufferFn(
      commandBuffer, srcBuffer, dstBuffer, regionCount, pRegions);
}
ALWAYS_INLINE void vkCmdCopyBufferToImage(VkCommandBuffer commandBuffer,
                                          VkBuffer srcBuffer,
                                          VkImage dstImage,
                                          VkImageLayout dstImageLayout,
                                          uint32_t regionCount,
                                          const VkBufferImageCopy* pRegions) {
  return gpu::GetVulkanFunctionPointers()->vkCmdCopyBufferToImageFn(
      commandBuffer, srcBuffer, dstImage, dstImageLayout, regionCount,
      pRegions);
}
ALWAYS_INLINE void vkCmdEndRenderPass(VkCommandBuffer commandBuffer) {
  return gpu::GetVulkanFunctionPointers()->vkCmdEndRenderPassFn(commandBuffer);
}
ALWAYS_INLINE void vkCmdExecuteCommands(
    VkCommandBuffer commandBuffer,
    uint32_t commandBufferCount,
    const VkCommandBuffer* pCommandBuffers) {
  return gpu::GetVulkanFunctionPointers()->vkCmdExecuteCommandsFn(
      commandBuffer, commandBufferCount, pCommandBuffers);
}
ALWAYS_INLINE void vkCmdNextSubpass(VkCommandBuffer commandBuffer,
                                    VkSubpassContents contents) {
  return gpu::GetVulkanFunctionPointers()->vkCmdNextSubpassFn(commandBuffer,
                                                              contents);
}
ALWAYS_INLINE void vkCmdPipelineBarrier(
    VkCommandBuffer commandBuffer,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkDependencyFlags dependencyFlags,
    uint32_t memoryBarrierCount,
    const VkMemoryBarrier* pMemoryBarriers,
    uint32_t bufferMemoryBarrierCount,
    const VkBufferMemoryBarrier* pBufferMemoryBarriers,
    uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier* pImageMemoryBarriers) {
  return gpu::GetVulkanFunctionPointers()->vkCmdPipelineBarrierFn(
      commandBuffer, srcStageMask, dstStageMask, dependencyFlags,
      memoryBarrierCount, pMemoryBarriers, bufferMemoryBarrierCount,
      pBufferMemoryBarriers, imageMemoryBarrierCount, pImageMemoryBarriers);
}
ALWAYS_INLINE VkResult vkCreateBuffer(VkDevice device,
                                      const VkBufferCreateInfo* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator,
                                      VkBuffer* pBuffer) {
  return gpu::GetVulkanFunctionPointers()->vkCreateBufferFn(
      device, pCreateInfo, pAllocator, pBuffer);
}
ALWAYS_INLINE VkResult
vkCreateCommandPool(VkDevice device,
                    const VkCommandPoolCreateInfo* pCreateInfo,
                    const VkAllocationCallbacks* pAllocator,
                    VkCommandPool* pCommandPool) {
  return gpu::GetVulkanFunctionPointers()->vkCreateCommandPoolFn(
      device, pCreateInfo, pAllocator, pCommandPool);
}
ALWAYS_INLINE VkResult
vkCreateDescriptorPool(VkDevice device,
                       const VkDescriptorPoolCreateInfo* pCreateInfo,
                       const VkAllocationCallbacks* pAllocator,
                       VkDescriptorPool* pDescriptorPool) {
  return gpu::GetVulkanFunctionPointers()->vkCreateDescriptorPoolFn(
      device, pCreateInfo, pAllocator, pDescriptorPool);
}
ALWAYS_INLINE VkResult
vkCreateDescriptorSetLayout(VkDevice device,
                            const VkDescriptorSetLayoutCreateInfo* pCreateInfo,
                            const VkAllocationCallbacks* pAllocator,
                            VkDescriptorSetLayout* pSetLayout) {
  return gpu::GetVulkanFunctionPointers()->vkCreateDescriptorSetLayoutFn(
      device, pCreateInfo, pAllocator, pSetLayout);
}
ALWAYS_INLINE VkResult vkCreateFence(VkDevice device,
                                     const VkFenceCreateInfo* pCreateInfo,
                                     const VkAllocationCallbacks* pAllocator,
                                     VkFence* pFence) {
  return gpu::GetVulkanFunctionPointers()->vkCreateFenceFn(device, pCreateInfo,
                                                           pAllocator, pFence);
}
ALWAYS_INLINE VkResult
vkCreateFramebuffer(VkDevice device,
                    const VkFramebufferCreateInfo* pCreateInfo,
                    const VkAllocationCallbacks* pAllocator,
                    VkFramebuffer* pFramebuffer) {
  return gpu::GetVulkanFunctionPointers()->vkCreateFramebufferFn(
      device, pCreateInfo, pAllocator, pFramebuffer);
}
ALWAYS_INLINE VkResult vkCreateImage(VkDevice device,
                                     const VkImageCreateInfo* pCreateInfo,
                                     const VkAllocationCallbacks* pAllocator,
                                     VkImage* pImage) {
  return gpu::GetVulkanFunctionPointers()->vkCreateImageFn(device, pCreateInfo,
                                                           pAllocator, pImage);
}
ALWAYS_INLINE VkResult
vkCreateImageView(VkDevice device,
                  const VkImageViewCreateInfo* pCreateInfo,
                  const VkAllocationCallbacks* pAllocator,
                  VkImageView* pView) {
  return gpu::GetVulkanFunctionPointers()->vkCreateImageViewFn(
      device, pCreateInfo, pAllocator, pView);
}
ALWAYS_INLINE VkResult
vkCreateRenderPass(VkDevice device,
                   const VkRenderPassCreateInfo* pCreateInfo,
                   const VkAllocationCallbacks* pAllocator,
                   VkRenderPass* pRenderPass) {
  return gpu::GetVulkanFunctionPointers()->vkCreateRenderPassFn(
      device, pCreateInfo, pAllocator, pRenderPass);
}
ALWAYS_INLINE VkResult vkCreateSampler(VkDevice device,
                                       const VkSamplerCreateInfo* pCreateInfo,
                                       const VkAllocationCallbacks* pAllocator,
                                       VkSampler* pSampler) {
  return gpu::GetVulkanFunctionPointers()->vkCreateSamplerFn(
      device, pCreateInfo, pAllocator, pSampler);
}
ALWAYS_INLINE VkResult
vkCreateSemaphore(VkDevice device,
                  const VkSemaphoreCreateInfo* pCreateInfo,
                  const VkAllocationCallbacks* pAllocator,
                  VkSemaphore* pSemaphore) {
  return gpu::GetVulkanFunctionPointers()->vkCreateSemaphoreFn(
      device, pCreateInfo, pAllocator, pSemaphore);
}
ALWAYS_INLINE VkResult
vkCreateShaderModule(VkDevice device,
                     const VkShaderModuleCreateInfo* pCreateInfo,
                     const VkAllocationCallbacks* pAllocator,
                     VkShaderModule* pShaderModule) {
  return gpu::GetVulkanFunctionPointers()->vkCreateShaderModuleFn(
      device, pCreateInfo, pAllocator, pShaderModule);
}
ALWAYS_INLINE void vkDestroyBuffer(VkDevice device,
                                   VkBuffer buffer,
                                   const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyBufferFn(device, buffer,
                                                             pAllocator);
}
ALWAYS_INLINE void vkDestroyCommandPool(
    VkDevice device,
    VkCommandPool commandPool,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyCommandPoolFn(
      device, commandPool, pAllocator);
}
ALWAYS_INLINE void vkDestroyDescriptorPool(
    VkDevice device,
    VkDescriptorPool descriptorPool,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyDescriptorPoolFn(
      device, descriptorPool, pAllocator);
}
ALWAYS_INLINE void vkDestroyDescriptorSetLayout(
    VkDevice device,
    VkDescriptorSetLayout descriptorSetLayout,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyDescriptorSetLayoutFn(
      device, descriptorSetLayout, pAllocator);
}
ALWAYS_INLINE void vkDestroyDevice(VkDevice device,
                                   const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyDeviceFn(device,
                                                             pAllocator);
}
ALWAYS_INLINE void vkDestroyFence(VkDevice device,
                                  VkFence fence,
                                  const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyFenceFn(device, fence,
                                                            pAllocator);
}
ALWAYS_INLINE void vkDestroyFramebuffer(
    VkDevice device,
    VkFramebuffer framebuffer,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyFramebufferFn(
      device, framebuffer, pAllocator);
}
ALWAYS_INLINE void vkDestroyImage(VkDevice device,
                                  VkImage image,
                                  const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyImageFn(device, image,
                                                            pAllocator);
}
ALWAYS_INLINE void vkDestroyImageView(VkDevice device,
                                      VkImageView imageView,
                                      const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyImageViewFn(
      device, imageView, pAllocator);
}
ALWAYS_INLINE void vkDestroyRenderPass(
    VkDevice device,
    VkRenderPass renderPass,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyRenderPassFn(
      device, renderPass, pAllocator);
}
ALWAYS_INLINE void vkDestroySampler(VkDevice device,
                                    VkSampler sampler,
                                    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroySamplerFn(device, sampler,
                                                              pAllocator);
}
ALWAYS_INLINE void vkDestroySemaphore(VkDevice device,
                                      VkSemaphore semaphore,
                                      const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroySemaphoreFn(
      device, semaphore, pAllocator);
}
ALWAYS_INLINE void vkDestroyShaderModule(
    VkDevice device,
    VkShaderModule shaderModule,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroyShaderModuleFn(
      device, shaderModule, pAllocator);
}
ALWAYS_INLINE VkResult vkDeviceWaitIdle(VkDevice device) {
  return gpu::GetVulkanFunctionPointers()->vkDeviceWaitIdleFn(device);
}
ALWAYS_INLINE VkResult
vkFlushMappedMemoryRanges(VkDevice device,
                          uint32_t memoryRangeCount,
                          const VkMappedMemoryRange* pMemoryRanges) {
  return gpu::GetVulkanFunctionPointers()->vkFlushMappedMemoryRangesFn(
      device, memoryRangeCount, pMemoryRanges);
}
ALWAYS_INLINE VkResult vkEndCommandBuffer(VkCommandBuffer commandBuffer) {
  return gpu::GetVulkanFunctionPointers()->vkEndCommandBufferFn(commandBuffer);
}
ALWAYS_INLINE void vkFreeCommandBuffers(
    VkDevice device,
    VkCommandPool commandPool,
    uint32_t commandBufferCount,
    const VkCommandBuffer* pCommandBuffers) {
  return gpu::GetVulkanFunctionPointers()->vkFreeCommandBuffersFn(
      device, commandPool, commandBufferCount, pCommandBuffers);
}
ALWAYS_INLINE VkResult
vkFreeDescriptorSets(VkDevice device,
                     VkDescriptorPool descriptorPool,
                     uint32_t descriptorSetCount,
                     const VkDescriptorSet* pDescriptorSets) {
  return gpu::GetVulkanFunctionPointers()->vkFreeDescriptorSetsFn(
      device, descriptorPool, descriptorSetCount, pDescriptorSets);
}
ALWAYS_INLINE void vkFreeMemory(VkDevice device,
                                VkDeviceMemory memory,
                                const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkFreeMemoryFn(device, memory,
                                                          pAllocator);
}
ALWAYS_INLINE VkResult
vkInvalidateMappedMemoryRanges(VkDevice device,
                               uint32_t memoryRangeCount,
                               const VkMappedMemoryRange* pMemoryRanges) {
  return gpu::GetVulkanFunctionPointers()->vkInvalidateMappedMemoryRangesFn(
      device, memoryRangeCount, pMemoryRanges);
}
ALWAYS_INLINE void vkGetBufferMemoryRequirements(
    VkDevice device,
    VkBuffer buffer,
    VkMemoryRequirements* pMemoryRequirements) {
  return gpu::GetVulkanFunctionPointers()->vkGetBufferMemoryRequirementsFn(
      device, buffer, pMemoryRequirements);
}
ALWAYS_INLINE void vkGetDeviceQueue(VkDevice device,
                                    uint32_t queueFamilyIndex,
                                    uint32_t queueIndex,
                                    VkQueue* pQueue) {
  return gpu::GetVulkanFunctionPointers()->vkGetDeviceQueueFn(
      device, queueFamilyIndex, queueIndex, pQueue);
}
ALWAYS_INLINE VkResult vkGetFenceStatus(VkDevice device, VkFence fence) {
  return gpu::GetVulkanFunctionPointers()->vkGetFenceStatusFn(device, fence);
}
ALWAYS_INLINE void vkGetImageMemoryRequirements(
    VkDevice device,
    VkImage image,
    VkMemoryRequirements* pMemoryRequirements) {
  return gpu::GetVulkanFunctionPointers()->vkGetImageMemoryRequirementsFn(
      device, image, pMemoryRequirements);
}
ALWAYS_INLINE VkResult vkMapMemory(VkDevice device,
                                   VkDeviceMemory memory,
                                   VkDeviceSize offset,
                                   VkDeviceSize size,
                                   VkMemoryMapFlags flags,
                                   void** ppData) {
  return gpu::GetVulkanFunctionPointers()->vkMapMemoryFn(device, memory, offset,
                                                         size, flags, ppData);
}
ALWAYS_INLINE VkResult vkQueueSubmit(VkQueue queue,
                                     uint32_t submitCount,
                                     const VkSubmitInfo* pSubmits,
                                     VkFence fence) {
  return gpu::GetVulkanFunctionPointers()->vkQueueSubmitFn(queue, submitCount,
                                                           pSubmits, fence);
}
ALWAYS_INLINE VkResult vkQueueWaitIdle(VkQueue queue) {
  return gpu::GetVulkanFunctionPointers()->vkQueueWaitIdleFn(queue);
}
ALWAYS_INLINE VkResult vkResetCommandBuffer(VkCommandBuffer commandBuffer,
                                            VkCommandBufferResetFlags flags) {
  return gpu::GetVulkanFunctionPointers()->vkResetCommandBufferFn(commandBuffer,
                                                                  flags);
}
ALWAYS_INLINE VkResult vkResetFences(VkDevice device,
                                     uint32_t fenceCount,
                                     const VkFence* pFences) {
  return gpu::GetVulkanFunctionPointers()->vkResetFencesFn(device, fenceCount,
                                                           pFences);
}
ALWAYS_INLINE void vkUnmapMemory(VkDevice device, VkDeviceMemory memory) {
  return gpu::GetVulkanFunctionPointers()->vkUnmapMemoryFn(device, memory);
}
ALWAYS_INLINE void vkUpdateDescriptorSets(
    VkDevice device,
    uint32_t descriptorWriteCount,
    const VkWriteDescriptorSet* pDescriptorWrites,
    uint32_t descriptorCopyCount,
    const VkCopyDescriptorSet* pDescriptorCopies) {
  return gpu::GetVulkanFunctionPointers()->vkUpdateDescriptorSetsFn(
      device, descriptorWriteCount, pDescriptorWrites, descriptorCopyCount,
      pDescriptorCopies);
}
ALWAYS_INLINE VkResult vkWaitForFences(VkDevice device,
                                       uint32_t fenceCount,
                                       const VkFence* pFences,
                                       VkBool32 waitAll,
                                       uint64_t timeout) {
  return gpu::GetVulkanFunctionPointers()->vkWaitForFencesFn(
      device, fenceCount, pFences, waitAll, timeout);
}

ALWAYS_INLINE void vkGetDeviceQueue2(VkDevice device,
                                     const VkDeviceQueueInfo2* pQueueInfo,
                                     VkQueue* pQueue) {
  return gpu::GetVulkanFunctionPointers()->vkGetDeviceQueue2Fn(
      device, pQueueInfo, pQueue);
}
ALWAYS_INLINE void vkGetBufferMemoryRequirements2(
    VkDevice device,
    const VkBufferMemoryRequirementsInfo2* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
  return gpu::GetVulkanFunctionPointers()->vkGetBufferMemoryRequirements2Fn(
      device, pInfo, pMemoryRequirements);
}
ALWAYS_INLINE void vkGetImageMemoryRequirements2(
    VkDevice device,
    const VkImageMemoryRequirementsInfo2* pInfo,
    VkMemoryRequirements2* pMemoryRequirements) {
  return gpu::GetVulkanFunctionPointers()->vkGetImageMemoryRequirements2Fn(
      device, pInfo, pMemoryRequirements);
}

#if defined(OS_ANDROID)
ALWAYS_INLINE VkResult vkGetAndroidHardwareBufferPropertiesANDROID(
    VkDevice device,
    const struct AHardwareBuffer* buffer,
    VkAndroidHardwareBufferPropertiesANDROID* pProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetAndroidHardwareBufferPropertiesANDROIDFn(device, buffer,
                                                      pProperties);
}
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
ALWAYS_INLINE VkResult
vkGetSemaphoreFdKHR(VkDevice device,
                    const VkSemaphoreGetFdInfoKHR* pGetFdInfo,
                    int* pFd) {
  return gpu::GetVulkanFunctionPointers()->vkGetSemaphoreFdKHRFn(
      device, pGetFdInfo, pFd);
}
ALWAYS_INLINE VkResult vkImportSemaphoreFdKHR(
    VkDevice device,
    const VkImportSemaphoreFdInfoKHR* pImportSemaphoreFdInfo) {
  return gpu::GetVulkanFunctionPointers()->vkImportSemaphoreFdKHRFn(
      device, pImportSemaphoreFdInfo);
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_WIN)
ALWAYS_INLINE VkResult vkGetSemaphoreWin32HandleKHR(
    VkDevice device,
    const VkSemaphoreGetWin32HandleInfoKHR* pGetWin32HandleInfo,
    HANDLE* pHandle) {
  return gpu::GetVulkanFunctionPointers()->vkGetSemaphoreWin32HandleKHRFn(
      device, pGetWin32HandleInfo, pHandle);
}
ALWAYS_INLINE VkResult
vkImportSemaphoreWin32HandleKHR(VkDevice device,
                                const VkImportSemaphoreWin32HandleInfoKHR*
                                    pImportSemaphoreWin32HandleInfo) {
  return gpu::GetVulkanFunctionPointers()->vkImportSemaphoreWin32HandleKHRFn(
      device, pImportSemaphoreWin32HandleInfo);
}
#endif  // defined(OS_WIN)

#if defined(OS_LINUX) || defined(OS_ANDROID)
ALWAYS_INLINE VkResult vkGetMemoryFdKHR(VkDevice device,
                                        const VkMemoryGetFdInfoKHR* pGetFdInfo,
                                        int* pFd) {
  return gpu::GetVulkanFunctionPointers()->vkGetMemoryFdKHRFn(device,
                                                              pGetFdInfo, pFd);
}
ALWAYS_INLINE VkResult
vkGetMemoryFdPropertiesKHR(VkDevice device,
                           VkExternalMemoryHandleTypeFlagBits handleType,
                           int fd,
                           VkMemoryFdPropertiesKHR* pMemoryFdProperties) {
  return gpu::GetVulkanFunctionPointers()->vkGetMemoryFdPropertiesKHRFn(
      device, handleType, fd, pMemoryFdProperties);
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_WIN)
ALWAYS_INLINE VkResult vkGetMemoryWin32HandleKHR(
    VkDevice device,
    const VkMemoryGetWin32HandleInfoKHR* pGetWin32HandleInfo,
    HANDLE* pHandle) {
  return gpu::GetVulkanFunctionPointers()->vkGetMemoryWin32HandleKHRFn(
      device, pGetWin32HandleInfo, pHandle);
}
ALWAYS_INLINE VkResult vkGetMemoryWin32HandlePropertiesKHR(
    VkDevice device,
    VkExternalMemoryHandleTypeFlagBits handleType,
    HANDLE handle,
    VkMemoryWin32HandlePropertiesKHR* pMemoryWin32HandleProperties) {
  return gpu::GetVulkanFunctionPointers()
      ->vkGetMemoryWin32HandlePropertiesKHRFn(device, handleType, handle,
                                              pMemoryWin32HandleProperties);
}
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

ALWAYS_INLINE VkResult vkAcquireNextImageKHR(VkDevice device,
                                             VkSwapchainKHR swapchain,
                                             uint64_t timeout,
                                             VkSemaphore semaphore,
                                             VkFence fence,
                                             uint32_t* pImageIndex) {
  return gpu::GetVulkanFunctionPointers()->vkAcquireNextImageKHRFn(
      device, swapchain, timeout, semaphore, fence, pImageIndex);
}
ALWAYS_INLINE VkResult
vkCreateSwapchainKHR(VkDevice device,
                     const VkSwapchainCreateInfoKHR* pCreateInfo,
                     const VkAllocationCallbacks* pAllocator,
                     VkSwapchainKHR* pSwapchain) {
  return gpu::GetVulkanFunctionPointers()->vkCreateSwapchainKHRFn(
      device, pCreateInfo, pAllocator, pSwapchain);
}
ALWAYS_INLINE void vkDestroySwapchainKHR(
    VkDevice device,
    VkSwapchainKHR swapchain,
    const VkAllocationCallbacks* pAllocator) {
  return gpu::GetVulkanFunctionPointers()->vkDestroySwapchainKHRFn(
      device, swapchain, pAllocator);
}
ALWAYS_INLINE VkResult vkGetSwapchainImagesKHR(VkDevice device,
                                               VkSwapchainKHR swapchain,
                                               uint32_t* pSwapchainImageCount,
                                               VkImage* pSwapchainImages) {
  return gpu::GetVulkanFunctionPointers()->vkGetSwapchainImagesKHRFn(
      device, swapchain, pSwapchainImageCount, pSwapchainImages);
}
ALWAYS_INLINE VkResult vkQueuePresentKHR(VkQueue queue,
                                         const VkPresentInfoKHR* pPresentInfo) {
  return gpu::GetVulkanFunctionPointers()->vkQueuePresentKHRFn(queue,
                                                               pPresentInfo);
}

#endif  // GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_