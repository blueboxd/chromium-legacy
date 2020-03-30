// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

#include "gpu/vulkan/vulkan_function_pointers.h"

#include "base/no_destructor.h"

namespace gpu {

VulkanFunctionPointers* GetVulkanFunctionPointers() {
  static base::NoDestructor<VulkanFunctionPointers> vulkan_function_pointers;
  return vulkan_function_pointers.get();
}

VulkanFunctionPointers::VulkanFunctionPointers() = default;
VulkanFunctionPointers::~VulkanFunctionPointers() = default;

NO_SANITIZE("cfi-icall")
bool VulkanFunctionPointers::BindUnassociatedFunctionPointers() {
  // vkGetInstanceProcAddr must be handled specially since it gets its function
  // pointer through base::GetFunctionPOinterFromNativeLibrary(). Other Vulkan
  // functions don't do this.
  vkGetInstanceProcAddrFn = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(vulkan_loader_library,
                                                "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddrFn)
    return false;

  vkEnumerateInstanceVersionFn =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
          vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
  // vkEnumerateInstanceVersion didn't exist in Vulkan 1.0, so we should
  // proceed even if we fail to get vkEnumerateInstanceVersion pointer.
  vkCreateInstanceFn = reinterpret_cast<PFN_vkCreateInstance>(
      vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
  if (!vkCreateInstanceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateInstance";
    return false;
  }

  vkEnumerateInstanceExtensionPropertiesFn =
      reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
          vkGetInstanceProcAddr(nullptr,
                                "vkEnumerateInstanceExtensionProperties"));
  if (!vkEnumerateInstanceExtensionPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateInstanceExtensionProperties";
    return false;
  }

  vkEnumerateInstanceLayerPropertiesFn =
      reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
          vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceLayerProperties"));
  if (!vkEnumerateInstanceLayerPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateInstanceLayerProperties";
    return false;
  }

  return true;
}

bool VulkanFunctionPointers::BindInstanceFunctionPointers(
    VkInstance vk_instance,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
  vkCreateDeviceFn = reinterpret_cast<PFN_vkCreateDevice>(
      vkGetInstanceProcAddr(vk_instance, "vkCreateDevice"));
  if (!vkCreateDeviceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDevice";
    return false;
  }

  vkDestroyInstanceFn = reinterpret_cast<PFN_vkDestroyInstance>(
      vkGetInstanceProcAddr(vk_instance, "vkDestroyInstance"));
  if (!vkDestroyInstanceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyInstance";
    return false;
  }

  vkEnumerateDeviceExtensionPropertiesFn =
      reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkEnumerateDeviceExtensionProperties"));
  if (!vkEnumerateDeviceExtensionPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateDeviceExtensionProperties";
    return false;
  }

  vkEnumerateDeviceLayerPropertiesFn =
      reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkEnumerateDeviceLayerProperties"));
  if (!vkEnumerateDeviceLayerPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateDeviceLayerProperties";
    return false;
  }

  vkEnumeratePhysicalDevicesFn =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          vkGetInstanceProcAddr(vk_instance, "vkEnumeratePhysicalDevices"));
  if (!vkEnumeratePhysicalDevicesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumeratePhysicalDevices";
    return false;
  }

  vkGetDeviceProcAddrFn = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      vkGetInstanceProcAddr(vk_instance, "vkGetDeviceProcAddr"));
  if (!vkGetDeviceProcAddrFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetDeviceProcAddr";
    return false;
  }

  vkGetPhysicalDeviceFeaturesFn =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(
          vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures"));
  if (!vkGetPhysicalDeviceFeaturesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceFeatures";
    return false;
  }

  vkGetPhysicalDeviceFormatPropertiesFn =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceFormatProperties"));
  if (!vkGetPhysicalDeviceFormatPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceFormatProperties";
    return false;
  }

  vkGetPhysicalDeviceMemoryPropertiesFn =
      reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceMemoryProperties"));
  if (!vkGetPhysicalDeviceMemoryPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceMemoryProperties";
    return false;
  }

  vkGetPhysicalDevicePropertiesFn =
      reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
          vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceProperties"));
  if (!vkGetPhysicalDevicePropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceProperties";
    return false;
  }

  vkGetPhysicalDeviceQueueFamilyPropertiesFn =
      reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceQueueFamilyProperties"));
  if (!vkGetPhysicalDeviceQueueFamilyPropertiesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceQueueFamilyProperties";
    return false;
  }

#if DCHECK_IS_ON()
  if (gfx::HasExtension(enabled_extensions,
                        VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
    vkCreateDebugReportCallbackEXTFn =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkCreateDebugReportCallbackEXT"));
    if (!vkCreateDebugReportCallbackEXTFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateDebugReportCallbackEXT";
      return false;
    }

    vkDestroyDebugReportCallbackEXTFn =
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkDestroyDebugReportCallbackEXT"));
    if (!vkDestroyDebugReportCallbackEXTFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroyDebugReportCallbackEXT";
      return false;
    }
  }
#endif  // DCHECK_IS_ON()

  if (gfx::HasExtension(enabled_extensions, VK_KHR_SURFACE_EXTENSION_NAME)) {
    vkDestroySurfaceKHRFn = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, "vkDestroySurfaceKHR"));
    if (!vkDestroySurfaceKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroySurfaceKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    if (!vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceFormatsKHRFn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    if (!vkGetPhysicalDeviceSurfaceFormatsKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceFormatsKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceSupportKHRFn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceSurfaceSupportKHR"));
    if (!vkGetPhysicalDeviceSurfaceSupportKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceSupportKHR";
      return false;
    }
  }

#if defined(USE_VULKAN_XLIB)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_XLIB_SURFACE_EXTENSION_NAME)) {
    vkCreateXlibSurfaceKHRFn = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, "vkCreateXlibSurfaceKHR"));
    if (!vkCreateXlibSurfaceKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateXlibSurfaceKHR";
      return false;
    }

    vkGetPhysicalDeviceXlibPresentationSupportKHRFn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR>(
            vkGetInstanceProcAddr(
                vk_instance, "vkGetPhysicalDeviceXlibPresentationSupportKHR"));
    if (!vkGetPhysicalDeviceXlibPresentationSupportKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceXlibPresentationSupportKHR";
      return false;
    }
  }
#endif  // defined(USE_VULKAN_XLIB)

#if defined(OS_WIN)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_WIN32_SURFACE_EXTENSION_NAME)) {
    vkCreateWin32SurfaceKHRFn = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, "vkCreateWin32SurfaceKHR"));
    if (!vkCreateWin32SurfaceKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateWin32SurfaceKHR";
      return false;
    }

    vkGetPhysicalDeviceWin32PresentationSupportKHRFn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceWin32PresentationSupportKHR>(
            vkGetInstanceProcAddr(
                vk_instance, "vkGetPhysicalDeviceWin32PresentationSupportKHR"));
    if (!vkGetPhysicalDeviceWin32PresentationSupportKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceWin32PresentationSupportKHR";
      return false;
    }
  }
#endif  // defined(OS_WIN)

#if defined(OS_ANDROID)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
    vkCreateAndroidSurfaceKHRFn =
        reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
            vkGetInstanceProcAddr(vk_instance, "vkCreateAndroidSurfaceKHR"));
    if (!vkCreateAndroidSurfaceKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateAndroidSurfaceKHR";
      return false;
    }
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME)) {
    vkCreateImagePipeSurfaceFUCHSIAFn =
        reinterpret_cast<PFN_vkCreateImagePipeSurfaceFUCHSIA>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkCreateImagePipeSurfaceFUCHSIA"));
    if (!vkCreateImagePipeSurfaceFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateImagePipeSurfaceFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

  if (api_version >= VK_API_VERSION_1_1) {
    vkGetPhysicalDeviceImageFormatProperties2Fn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceImageFormatProperties2"));
    if (!vkGetPhysicalDeviceImageFormatProperties2Fn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceImageFormatProperties2";
      return false;
    }
  }

  if (api_version >= VK_API_VERSION_1_1) {
    vkGetPhysicalDeviceFeatures2Fn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures2"));
    if (!vkGetPhysicalDeviceFeatures2Fn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceFeatures2";
      return false;
    }

  } else if (gfx::HasExtension(
                 enabled_extensions,
                 VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
    vkGetPhysicalDeviceFeatures2Fn =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceFeatures2KHR"));
    if (!vkGetPhysicalDeviceFeatures2Fn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceFeatures2KHR";
      return false;
    }
  }

  return true;
}

bool VulkanFunctionPointers::BindDeviceFunctionPointers(
    VkDevice vk_device,
    uint32_t api_version,
    const gfx::ExtensionSet& enabled_extensions) {
  // Device functions
  vkAllocateCommandBuffersFn = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateCommandBuffers"));
  if (!vkAllocateCommandBuffersFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateCommandBuffers";
    return false;
  }

  vkAllocateDescriptorSetsFn = reinterpret_cast<PFN_vkAllocateDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateDescriptorSets"));
  if (!vkAllocateDescriptorSetsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateDescriptorSets";
    return false;
  }

  vkAllocateMemoryFn = reinterpret_cast<PFN_vkAllocateMemory>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateMemory"));
  if (!vkAllocateMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateMemory";
    return false;
  }

  vkBeginCommandBufferFn = reinterpret_cast<PFN_vkBeginCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkBeginCommandBuffer"));
  if (!vkBeginCommandBufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBeginCommandBuffer";
    return false;
  }

  vkBindBufferMemoryFn = reinterpret_cast<PFN_vkBindBufferMemory>(
      vkGetDeviceProcAddr(vk_device, "vkBindBufferMemory"));
  if (!vkBindBufferMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBindBufferMemory";
    return false;
  }

  vkBindImageMemoryFn = reinterpret_cast<PFN_vkBindImageMemory>(
      vkGetDeviceProcAddr(vk_device, "vkBindImageMemory"));
  if (!vkBindImageMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBindImageMemory";
    return false;
  }

  vkCmdBeginRenderPassFn = reinterpret_cast<PFN_vkCmdBeginRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdBeginRenderPass"));
  if (!vkCmdBeginRenderPassFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdBeginRenderPass";
    return false;
  }

  vkCmdCopyBufferToImageFn = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(
      vkGetDeviceProcAddr(vk_device, "vkCmdCopyBufferToImage"));
  if (!vkCmdCopyBufferToImageFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdCopyBufferToImage";
    return false;
  }

  vkCmdEndRenderPassFn = reinterpret_cast<PFN_vkCmdEndRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdEndRenderPass"));
  if (!vkCmdEndRenderPassFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdEndRenderPass";
    return false;
  }

  vkCmdExecuteCommandsFn = reinterpret_cast<PFN_vkCmdExecuteCommands>(
      vkGetDeviceProcAddr(vk_device, "vkCmdExecuteCommands"));
  if (!vkCmdExecuteCommandsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdExecuteCommands";
    return false;
  }

  vkCmdNextSubpassFn = reinterpret_cast<PFN_vkCmdNextSubpass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdNextSubpass"));
  if (!vkCmdNextSubpassFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdNextSubpass";
    return false;
  }

  vkCmdPipelineBarrierFn = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
      vkGetDeviceProcAddr(vk_device, "vkCmdPipelineBarrier"));
  if (!vkCmdPipelineBarrierFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdPipelineBarrier";
    return false;
  }

  vkCreateBufferFn = reinterpret_cast<PFN_vkCreateBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkCreateBuffer"));
  if (!vkCreateBufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateBuffer";
    return false;
  }

  vkCreateCommandPoolFn = reinterpret_cast<PFN_vkCreateCommandPool>(
      vkGetDeviceProcAddr(vk_device, "vkCreateCommandPool"));
  if (!vkCreateCommandPoolFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateCommandPool";
    return false;
  }

  vkCreateDescriptorPoolFn = reinterpret_cast<PFN_vkCreateDescriptorPool>(
      vkGetDeviceProcAddr(vk_device, "vkCreateDescriptorPool"));
  if (!vkCreateDescriptorPoolFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDescriptorPool";
    return false;
  }

  vkCreateDescriptorSetLayoutFn =
      reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
          vkGetDeviceProcAddr(vk_device, "vkCreateDescriptorSetLayout"));
  if (!vkCreateDescriptorSetLayoutFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDescriptorSetLayout";
    return false;
  }

  vkCreateFenceFn = reinterpret_cast<PFN_vkCreateFence>(
      vkGetDeviceProcAddr(vk_device, "vkCreateFence"));
  if (!vkCreateFenceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateFence";
    return false;
  }

  vkCreateFramebufferFn = reinterpret_cast<PFN_vkCreateFramebuffer>(
      vkGetDeviceProcAddr(vk_device, "vkCreateFramebuffer"));
  if (!vkCreateFramebufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateFramebuffer";
    return false;
  }

  vkCreateImageFn = reinterpret_cast<PFN_vkCreateImage>(
      vkGetDeviceProcAddr(vk_device, "vkCreateImage"));
  if (!vkCreateImageFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateImage";
    return false;
  }

  vkCreateImageViewFn = reinterpret_cast<PFN_vkCreateImageView>(
      vkGetDeviceProcAddr(vk_device, "vkCreateImageView"));
  if (!vkCreateImageViewFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateImageView";
    return false;
  }

  vkCreateRenderPassFn = reinterpret_cast<PFN_vkCreateRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCreateRenderPass"));
  if (!vkCreateRenderPassFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateRenderPass";
    return false;
  }

  vkCreateSamplerFn = reinterpret_cast<PFN_vkCreateSampler>(
      vkGetDeviceProcAddr(vk_device, "vkCreateSampler"));
  if (!vkCreateSamplerFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateSampler";
    return false;
  }

  vkCreateSemaphoreFn = reinterpret_cast<PFN_vkCreateSemaphore>(
      vkGetDeviceProcAddr(vk_device, "vkCreateSemaphore"));
  if (!vkCreateSemaphoreFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateSemaphore";
    return false;
  }

  vkCreateShaderModuleFn = reinterpret_cast<PFN_vkCreateShaderModule>(
      vkGetDeviceProcAddr(vk_device, "vkCreateShaderModule"));
  if (!vkCreateShaderModuleFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateShaderModule";
    return false;
  }

  vkDestroyBufferFn = reinterpret_cast<PFN_vkDestroyBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyBuffer"));
  if (!vkDestroyBufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyBuffer";
    return false;
  }

  vkDestroyCommandPoolFn = reinterpret_cast<PFN_vkDestroyCommandPool>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyCommandPool"));
  if (!vkDestroyCommandPoolFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyCommandPool";
    return false;
  }

  vkDestroyDescriptorPoolFn = reinterpret_cast<PFN_vkDestroyDescriptorPool>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyDescriptorPool"));
  if (!vkDestroyDescriptorPoolFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDescriptorPool";
    return false;
  }

  vkDestroyDescriptorSetLayoutFn =
      reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(
          vkGetDeviceProcAddr(vk_device, "vkDestroyDescriptorSetLayout"));
  if (!vkDestroyDescriptorSetLayoutFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDescriptorSetLayout";
    return false;
  }

  vkDestroyDeviceFn = reinterpret_cast<PFN_vkDestroyDevice>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyDevice"));
  if (!vkDestroyDeviceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDevice";
    return false;
  }

  vkDestroyFenceFn = reinterpret_cast<PFN_vkDestroyFence>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyFence"));
  if (!vkDestroyFenceFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyFence";
    return false;
  }

  vkDestroyFramebufferFn = reinterpret_cast<PFN_vkDestroyFramebuffer>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyFramebuffer"));
  if (!vkDestroyFramebufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyFramebuffer";
    return false;
  }

  vkDestroyImageFn = reinterpret_cast<PFN_vkDestroyImage>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyImage"));
  if (!vkDestroyImageFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyImage";
    return false;
  }

  vkDestroyImageViewFn = reinterpret_cast<PFN_vkDestroyImageView>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyImageView"));
  if (!vkDestroyImageViewFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyImageView";
    return false;
  }

  vkDestroyRenderPassFn = reinterpret_cast<PFN_vkDestroyRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyRenderPass"));
  if (!vkDestroyRenderPassFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyRenderPass";
    return false;
  }

  vkDestroySamplerFn = reinterpret_cast<PFN_vkDestroySampler>(
      vkGetDeviceProcAddr(vk_device, "vkDestroySampler"));
  if (!vkDestroySamplerFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroySampler";
    return false;
  }

  vkDestroySemaphoreFn = reinterpret_cast<PFN_vkDestroySemaphore>(
      vkGetDeviceProcAddr(vk_device, "vkDestroySemaphore"));
  if (!vkDestroySemaphoreFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroySemaphore";
    return false;
  }

  vkDestroyShaderModuleFn = reinterpret_cast<PFN_vkDestroyShaderModule>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyShaderModule"));
  if (!vkDestroyShaderModuleFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyShaderModule";
    return false;
  }

  vkDeviceWaitIdleFn = reinterpret_cast<PFN_vkDeviceWaitIdle>(
      vkGetDeviceProcAddr(vk_device, "vkDeviceWaitIdle"));
  if (!vkDeviceWaitIdleFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDeviceWaitIdle";
    return false;
  }

  vkEndCommandBufferFn = reinterpret_cast<PFN_vkEndCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkEndCommandBuffer"));
  if (!vkEndCommandBufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEndCommandBuffer";
    return false;
  }

  vkFreeCommandBuffersFn = reinterpret_cast<PFN_vkFreeCommandBuffers>(
      vkGetDeviceProcAddr(vk_device, "vkFreeCommandBuffers"));
  if (!vkFreeCommandBuffersFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeCommandBuffers";
    return false;
  }

  vkFreeDescriptorSetsFn = reinterpret_cast<PFN_vkFreeDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkFreeDescriptorSets"));
  if (!vkFreeDescriptorSetsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeDescriptorSets";
    return false;
  }

  vkFreeMemoryFn = reinterpret_cast<PFN_vkFreeMemory>(
      vkGetDeviceProcAddr(vk_device, "vkFreeMemory"));
  if (!vkFreeMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeMemory";
    return false;
  }

  vkGetBufferMemoryRequirementsFn =
      reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
          vkGetDeviceProcAddr(vk_device, "vkGetBufferMemoryRequirements"));
  if (!vkGetBufferMemoryRequirementsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetBufferMemoryRequirements";
    return false;
  }

  vkGetDeviceQueueFn = reinterpret_cast<PFN_vkGetDeviceQueue>(
      vkGetDeviceProcAddr(vk_device, "vkGetDeviceQueue"));
  if (!vkGetDeviceQueueFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetDeviceQueue";
    return false;
  }

  vkGetFenceStatusFn = reinterpret_cast<PFN_vkGetFenceStatus>(
      vkGetDeviceProcAddr(vk_device, "vkGetFenceStatus"));
  if (!vkGetFenceStatusFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetFenceStatus";
    return false;
  }

  vkGetImageMemoryRequirementsFn =
      reinterpret_cast<PFN_vkGetImageMemoryRequirements>(
          vkGetDeviceProcAddr(vk_device, "vkGetImageMemoryRequirements"));
  if (!vkGetImageMemoryRequirementsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetImageMemoryRequirements";
    return false;
  }

  vkMapMemoryFn = reinterpret_cast<PFN_vkMapMemory>(
      vkGetDeviceProcAddr(vk_device, "vkMapMemory"));
  if (!vkMapMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkMapMemory";
    return false;
  }

  vkQueueSubmitFn = reinterpret_cast<PFN_vkQueueSubmit>(
      vkGetDeviceProcAddr(vk_device, "vkQueueSubmit"));
  if (!vkQueueSubmitFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkQueueSubmit";
    return false;
  }

  vkQueueWaitIdleFn = reinterpret_cast<PFN_vkQueueWaitIdle>(
      vkGetDeviceProcAddr(vk_device, "vkQueueWaitIdle"));
  if (!vkQueueWaitIdleFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkQueueWaitIdle";
    return false;
  }

  vkResetCommandBufferFn = reinterpret_cast<PFN_vkResetCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkResetCommandBuffer"));
  if (!vkResetCommandBufferFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkResetCommandBuffer";
    return false;
  }

  vkResetFencesFn = reinterpret_cast<PFN_vkResetFences>(
      vkGetDeviceProcAddr(vk_device, "vkResetFences"));
  if (!vkResetFencesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkResetFences";
    return false;
  }

  vkUnmapMemoryFn = reinterpret_cast<PFN_vkUnmapMemory>(
      vkGetDeviceProcAddr(vk_device, "vkUnmapMemory"));
  if (!vkUnmapMemoryFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkUnmapMemory";
    return false;
  }

  vkUpdateDescriptorSetsFn = reinterpret_cast<PFN_vkUpdateDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkUpdateDescriptorSets"));
  if (!vkUpdateDescriptorSetsFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkUpdateDescriptorSets";
    return false;
  }

  vkWaitForFencesFn = reinterpret_cast<PFN_vkWaitForFences>(
      vkGetDeviceProcAddr(vk_device, "vkWaitForFences"));
  if (!vkWaitForFencesFn) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkWaitForFences";
    return false;
  }

  if (api_version >= VK_API_VERSION_1_1) {
    vkGetDeviceQueue2Fn = reinterpret_cast<PFN_vkGetDeviceQueue2>(
        vkGetDeviceProcAddr(vk_device, "vkGetDeviceQueue2"));
    if (!vkGetDeviceQueue2Fn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetDeviceQueue2";
      return false;
    }

    vkGetImageMemoryRequirements2Fn =
        reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(
            vkGetDeviceProcAddr(vk_device, "vkGetImageMemoryRequirements2"));
    if (!vkGetImageMemoryRequirements2Fn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetImageMemoryRequirements2";
      return false;
    }
  }

#if defined(OS_ANDROID)
  if (gfx::HasExtension(
          enabled_extensions,
          VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME)) {
    vkGetAndroidHardwareBufferPropertiesANDROIDFn =
        reinterpret_cast<PFN_vkGetAndroidHardwareBufferPropertiesANDROID>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetAndroidHardwareBufferPropertiesANDROID"));
    if (!vkGetAndroidHardwareBufferPropertiesANDROIDFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetAndroidHardwareBufferPropertiesANDROID";
      return false;
    }
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME)) {
    vkGetSemaphoreFdKHRFn = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
        vkGetDeviceProcAddr(vk_device, "vkGetSemaphoreFdKHR"));
    if (!vkGetSemaphoreFdKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSemaphoreFdKHR";
      return false;
    }

    vkImportSemaphoreFdKHRFn = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(
        vkGetDeviceProcAddr(vk_device, "vkImportSemaphoreFdKHR"));
    if (!vkImportSemaphoreFdKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkImportSemaphoreFdKHR";
      return false;
    }
  }
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)) {
    vkGetMemoryFdKHRFn = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        vkGetDeviceProcAddr(vk_device, "vkGetMemoryFdKHR"));
    if (!vkGetMemoryFdKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryFdKHR";
      return false;
    }

    vkGetMemoryFdPropertiesKHRFn =
        reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
            vkGetDeviceProcAddr(vk_device, "vkGetMemoryFdPropertiesKHR"));
    if (!vkGetMemoryFdPropertiesKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryFdPropertiesKHR";
      return false;
    }
  }
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME)) {
    vkImportSemaphoreZirconHandleFUCHSIAFn =
        reinterpret_cast<PFN_vkImportSemaphoreZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkImportSemaphoreZirconHandleFUCHSIA"));
    if (!vkImportSemaphoreZirconHandleFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkImportSemaphoreZirconHandleFUCHSIA";
      return false;
    }

    vkGetSemaphoreZirconHandleFUCHSIAFn =
        reinterpret_cast<PFN_vkGetSemaphoreZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetSemaphoreZirconHandleFUCHSIA"));
    if (!vkGetSemaphoreZirconHandleFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSemaphoreZirconHandleFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME)) {
    vkGetMemoryZirconHandleFUCHSIAFn =
        reinterpret_cast<PFN_vkGetMemoryZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, "vkGetMemoryZirconHandleFUCHSIA"));
    if (!vkGetMemoryZirconHandleFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryZirconHandleFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME)) {
    vkCreateBufferCollectionFUCHSIAFn =
        reinterpret_cast<PFN_vkCreateBufferCollectionFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, "vkCreateBufferCollectionFUCHSIA"));
    if (!vkCreateBufferCollectionFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateBufferCollectionFUCHSIA";
      return false;
    }

    vkSetBufferCollectionConstraintsFUCHSIAFn =
        reinterpret_cast<PFN_vkSetBufferCollectionConstraintsFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkSetBufferCollectionConstraintsFUCHSIA"));
    if (!vkSetBufferCollectionConstraintsFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkSetBufferCollectionConstraintsFUCHSIA";
      return false;
    }

    vkGetBufferCollectionPropertiesFUCHSIAFn =
        reinterpret_cast<PFN_vkGetBufferCollectionPropertiesFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetBufferCollectionPropertiesFUCHSIA"));
    if (!vkGetBufferCollectionPropertiesFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetBufferCollectionPropertiesFUCHSIA";
      return false;
    }

    vkDestroyBufferCollectionFUCHSIAFn =
        reinterpret_cast<PFN_vkDestroyBufferCollectionFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, "vkDestroyBufferCollectionFUCHSIA"));
    if (!vkDestroyBufferCollectionFUCHSIAFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroyBufferCollectionFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

  if (gfx::HasExtension(enabled_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
    vkAcquireNextImageKHRFn = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
        vkGetDeviceProcAddr(vk_device, "vkAcquireNextImageKHR"));
    if (!vkAcquireNextImageKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkAcquireNextImageKHR";
      return false;
    }

    vkCreateSwapchainKHRFn = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
        vkGetDeviceProcAddr(vk_device, "vkCreateSwapchainKHR"));
    if (!vkCreateSwapchainKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateSwapchainKHR";
      return false;
    }

    vkDestroySwapchainKHRFn = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
        vkGetDeviceProcAddr(vk_device, "vkDestroySwapchainKHR"));
    if (!vkDestroySwapchainKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroySwapchainKHR";
      return false;
    }

    vkGetSwapchainImagesKHRFn = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
        vkGetDeviceProcAddr(vk_device, "vkGetSwapchainImagesKHR"));
    if (!vkGetSwapchainImagesKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSwapchainImagesKHR";
      return false;
    }

    vkQueuePresentKHRFn = reinterpret_cast<PFN_vkQueuePresentKHR>(
        vkGetDeviceProcAddr(vk_device, "vkQueuePresentKHR"));
    if (!vkQueuePresentKHRFn) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkQueuePresentKHR";
      return false;
    }
  }

  return true;
}

}  // namespace gpu
