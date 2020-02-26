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
  vkGetInstanceProcAddrFn_ = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(vulkan_loader_library_,
                                                "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddrFn_)
    return false;

  vkEnumerateInstanceVersionFn_ =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
          vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
  // vkEnumerateInstanceVersion didn't exist in Vulkan 1.0, so we should
  // proceed even if we fail to get vkEnumerateInstanceVersion pointer.
  vkCreateInstanceFn_ = reinterpret_cast<PFN_vkCreateInstance>(
      vkGetInstanceProcAddr(nullptr, "vkCreateInstance"));
  if (!vkCreateInstanceFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateInstance";
    return false;
  }

  vkEnumerateInstanceExtensionPropertiesFn_ =
      reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(
          vkGetInstanceProcAddr(nullptr,
                                "vkEnumerateInstanceExtensionProperties"));
  if (!vkEnumerateInstanceExtensionPropertiesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateInstanceExtensionProperties";
    return false;
  }

  vkEnumerateInstanceLayerPropertiesFn_ =
      reinterpret_cast<PFN_vkEnumerateInstanceLayerProperties>(
          vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceLayerProperties"));
  if (!vkEnumerateInstanceLayerPropertiesFn_) {
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
  vkCreateDeviceFn_ = reinterpret_cast<PFN_vkCreateDevice>(
      vkGetInstanceProcAddr(vk_instance, "vkCreateDevice"));
  if (!vkCreateDeviceFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDevice";
    return false;
  }

  vkDestroyInstanceFn_ = reinterpret_cast<PFN_vkDestroyInstance>(
      vkGetInstanceProcAddr(vk_instance, "vkDestroyInstance"));
  if (!vkDestroyInstanceFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyInstance";
    return false;
  }

  vkEnumerateDeviceExtensionPropertiesFn_ =
      reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkEnumerateDeviceExtensionProperties"));
  if (!vkEnumerateDeviceExtensionPropertiesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateDeviceExtensionProperties";
    return false;
  }

  vkEnumerateDeviceLayerPropertiesFn_ =
      reinterpret_cast<PFN_vkEnumerateDeviceLayerProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkEnumerateDeviceLayerProperties"));
  if (!vkEnumerateDeviceLayerPropertiesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumerateDeviceLayerProperties";
    return false;
  }

  vkEnumeratePhysicalDevicesFn_ =
      reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
          vkGetInstanceProcAddr(vk_instance, "vkEnumeratePhysicalDevices"));
  if (!vkEnumeratePhysicalDevicesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEnumeratePhysicalDevices";
    return false;
  }

  vkGetDeviceProcAddrFn_ = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
      vkGetInstanceProcAddr(vk_instance, "vkGetDeviceProcAddr"));
  if (!vkGetDeviceProcAddrFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetDeviceProcAddr";
    return false;
  }

  vkGetPhysicalDeviceFeaturesFn_ =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures>(
          vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures"));
  if (!vkGetPhysicalDeviceFeaturesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceFeatures";
    return false;
  }

  vkGetPhysicalDeviceFormatPropertiesFn_ =
      reinterpret_cast<PFN_vkGetPhysicalDeviceFormatProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceFormatProperties"));
  if (!vkGetPhysicalDeviceFormatPropertiesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceFormatProperties";
    return false;
  }

  vkGetPhysicalDeviceMemoryPropertiesFn_ =
      reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceMemoryProperties"));
  if (!vkGetPhysicalDeviceMemoryPropertiesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceMemoryProperties";
    return false;
  }

  vkGetPhysicalDevicePropertiesFn_ =
      reinterpret_cast<PFN_vkGetPhysicalDeviceProperties>(
          vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceProperties"));
  if (!vkGetPhysicalDevicePropertiesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceProperties";
    return false;
  }

  vkGetPhysicalDeviceQueueFamilyPropertiesFn_ =
      reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
          vkGetInstanceProcAddr(vk_instance,
                                "vkGetPhysicalDeviceQueueFamilyProperties"));
  if (!vkGetPhysicalDeviceQueueFamilyPropertiesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetPhysicalDeviceQueueFamilyProperties";
    return false;
  }

#if DCHECK_IS_ON()
  if (gfx::HasExtension(enabled_extensions,
                        VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
    vkCreateDebugReportCallbackEXTFn_ =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkCreateDebugReportCallbackEXT"));
    if (!vkCreateDebugReportCallbackEXTFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateDebugReportCallbackEXT";
      return false;
    }

    vkDestroyDebugReportCallbackEXTFn_ =
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkDestroyDebugReportCallbackEXT"));
    if (!vkDestroyDebugReportCallbackEXTFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroyDebugReportCallbackEXT";
      return false;
    }
  }
#endif  // DCHECK_IS_ON()

  if (gfx::HasExtension(enabled_extensions, VK_KHR_SURFACE_EXTENSION_NAME)) {
    vkDestroySurfaceKHRFn_ = reinterpret_cast<PFN_vkDestroySurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, "vkDestroySurfaceKHR"));
    if (!vkDestroySurfaceKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroySurfaceKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn_ =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    if (!vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceCapabilitiesKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceFormatsKHRFn_ =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceSurfaceFormatsKHR"));
    if (!vkGetPhysicalDeviceSurfaceFormatsKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceFormatsKHR";
      return false;
    }

    vkGetPhysicalDeviceSurfaceSupportKHRFn_ =
        reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceSurfaceSupportKHR"));
    if (!vkGetPhysicalDeviceSurfaceSupportKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceSurfaceSupportKHR";
      return false;
    }
  }

#if defined(USE_VULKAN_XLIB)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_XLIB_SURFACE_EXTENSION_NAME)) {
    vkCreateXlibSurfaceKHRFn_ = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
        vkGetInstanceProcAddr(vk_instance, "vkCreateXlibSurfaceKHR"));
    if (!vkCreateXlibSurfaceKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateXlibSurfaceKHR";
      return false;
    }

    vkGetPhysicalDeviceXlibPresentationSupportKHRFn_ =
        reinterpret_cast<PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR>(
            vkGetInstanceProcAddr(
                vk_instance, "vkGetPhysicalDeviceXlibPresentationSupportKHR"));
    if (!vkGetPhysicalDeviceXlibPresentationSupportKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceXlibPresentationSupportKHR";
      return false;
    }
  }
#endif  // defined(USE_VULKAN_XLIB)

#if defined(OS_ANDROID)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
    vkCreateAndroidSurfaceKHRFn_ =
        reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
            vkGetInstanceProcAddr(vk_instance, "vkCreateAndroidSurfaceKHR"));
    if (!vkCreateAndroidSurfaceKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateAndroidSurfaceKHR";
      return false;
    }
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME)) {
    vkCreateImagePipeSurfaceFUCHSIAFn_ =
        reinterpret_cast<PFN_vkCreateImagePipeSurfaceFUCHSIA>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkCreateImagePipeSurfaceFUCHSIA"));
    if (!vkCreateImagePipeSurfaceFUCHSIAFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateImagePipeSurfaceFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

  if (api_version >= VK_API_VERSION_1_1) {
    vkGetPhysicalDeviceImageFormatProperties2Fn_ =
        reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties2>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceImageFormatProperties2"));
    if (!vkGetPhysicalDeviceImageFormatProperties2Fn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceImageFormatProperties2";
      return false;
    }
  }

  if (api_version >= VK_API_VERSION_1_1) {
    vkGetPhysicalDeviceFeatures2Fn_ =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            vkGetInstanceProcAddr(vk_instance, "vkGetPhysicalDeviceFeatures2"));
    if (!vkGetPhysicalDeviceFeatures2Fn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetPhysicalDeviceFeatures2";
      return false;
    }

  } else if (gfx::HasExtension(
                 enabled_extensions,
                 VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
    vkGetPhysicalDeviceFeatures2Fn_ =
        reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
            vkGetInstanceProcAddr(vk_instance,
                                  "vkGetPhysicalDeviceFeatures2KHR"));
    if (!vkGetPhysicalDeviceFeatures2Fn_) {
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
  vkAllocateCommandBuffersFn_ = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateCommandBuffers"));
  if (!vkAllocateCommandBuffersFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateCommandBuffers";
    return false;
  }

  vkAllocateDescriptorSetsFn_ = reinterpret_cast<PFN_vkAllocateDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateDescriptorSets"));
  if (!vkAllocateDescriptorSetsFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateDescriptorSets";
    return false;
  }

  vkAllocateMemoryFn_ = reinterpret_cast<PFN_vkAllocateMemory>(
      vkGetDeviceProcAddr(vk_device, "vkAllocateMemory"));
  if (!vkAllocateMemoryFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkAllocateMemory";
    return false;
  }

  vkBeginCommandBufferFn_ = reinterpret_cast<PFN_vkBeginCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkBeginCommandBuffer"));
  if (!vkBeginCommandBufferFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBeginCommandBuffer";
    return false;
  }

  vkBindBufferMemoryFn_ = reinterpret_cast<PFN_vkBindBufferMemory>(
      vkGetDeviceProcAddr(vk_device, "vkBindBufferMemory"));
  if (!vkBindBufferMemoryFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBindBufferMemory";
    return false;
  }

  vkBindImageMemoryFn_ = reinterpret_cast<PFN_vkBindImageMemory>(
      vkGetDeviceProcAddr(vk_device, "vkBindImageMemory"));
  if (!vkBindImageMemoryFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkBindImageMemory";
    return false;
  }

  vkCmdBeginRenderPassFn_ = reinterpret_cast<PFN_vkCmdBeginRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdBeginRenderPass"));
  if (!vkCmdBeginRenderPassFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdBeginRenderPass";
    return false;
  }

  vkCmdCopyBufferToImageFn_ = reinterpret_cast<PFN_vkCmdCopyBufferToImage>(
      vkGetDeviceProcAddr(vk_device, "vkCmdCopyBufferToImage"));
  if (!vkCmdCopyBufferToImageFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdCopyBufferToImage";
    return false;
  }

  vkCmdEndRenderPassFn_ = reinterpret_cast<PFN_vkCmdEndRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdEndRenderPass"));
  if (!vkCmdEndRenderPassFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdEndRenderPass";
    return false;
  }

  vkCmdExecuteCommandsFn_ = reinterpret_cast<PFN_vkCmdExecuteCommands>(
      vkGetDeviceProcAddr(vk_device, "vkCmdExecuteCommands"));
  if (!vkCmdExecuteCommandsFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdExecuteCommands";
    return false;
  }

  vkCmdNextSubpassFn_ = reinterpret_cast<PFN_vkCmdNextSubpass>(
      vkGetDeviceProcAddr(vk_device, "vkCmdNextSubpass"));
  if (!vkCmdNextSubpassFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdNextSubpass";
    return false;
  }

  vkCmdPipelineBarrierFn_ = reinterpret_cast<PFN_vkCmdPipelineBarrier>(
      vkGetDeviceProcAddr(vk_device, "vkCmdPipelineBarrier"));
  if (!vkCmdPipelineBarrierFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCmdPipelineBarrier";
    return false;
  }

  vkCreateBufferFn_ = reinterpret_cast<PFN_vkCreateBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkCreateBuffer"));
  if (!vkCreateBufferFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateBuffer";
    return false;
  }

  vkCreateCommandPoolFn_ = reinterpret_cast<PFN_vkCreateCommandPool>(
      vkGetDeviceProcAddr(vk_device, "vkCreateCommandPool"));
  if (!vkCreateCommandPoolFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateCommandPool";
    return false;
  }

  vkCreateDescriptorPoolFn_ = reinterpret_cast<PFN_vkCreateDescriptorPool>(
      vkGetDeviceProcAddr(vk_device, "vkCreateDescriptorPool"));
  if (!vkCreateDescriptorPoolFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDescriptorPool";
    return false;
  }

  vkCreateDescriptorSetLayoutFn_ =
      reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
          vkGetDeviceProcAddr(vk_device, "vkCreateDescriptorSetLayout"));
  if (!vkCreateDescriptorSetLayoutFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateDescriptorSetLayout";
    return false;
  }

  vkCreateFenceFn_ = reinterpret_cast<PFN_vkCreateFence>(
      vkGetDeviceProcAddr(vk_device, "vkCreateFence"));
  if (!vkCreateFenceFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateFence";
    return false;
  }

  vkCreateFramebufferFn_ = reinterpret_cast<PFN_vkCreateFramebuffer>(
      vkGetDeviceProcAddr(vk_device, "vkCreateFramebuffer"));
  if (!vkCreateFramebufferFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateFramebuffer";
    return false;
  }

  vkCreateImageFn_ = reinterpret_cast<PFN_vkCreateImage>(
      vkGetDeviceProcAddr(vk_device, "vkCreateImage"));
  if (!vkCreateImageFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateImage";
    return false;
  }

  vkCreateImageViewFn_ = reinterpret_cast<PFN_vkCreateImageView>(
      vkGetDeviceProcAddr(vk_device, "vkCreateImageView"));
  if (!vkCreateImageViewFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateImageView";
    return false;
  }

  vkCreateRenderPassFn_ = reinterpret_cast<PFN_vkCreateRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkCreateRenderPass"));
  if (!vkCreateRenderPassFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateRenderPass";
    return false;
  }

  vkCreateSamplerFn_ = reinterpret_cast<PFN_vkCreateSampler>(
      vkGetDeviceProcAddr(vk_device, "vkCreateSampler"));
  if (!vkCreateSamplerFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateSampler";
    return false;
  }

  vkCreateSemaphoreFn_ = reinterpret_cast<PFN_vkCreateSemaphore>(
      vkGetDeviceProcAddr(vk_device, "vkCreateSemaphore"));
  if (!vkCreateSemaphoreFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateSemaphore";
    return false;
  }

  vkCreateShaderModuleFn_ = reinterpret_cast<PFN_vkCreateShaderModule>(
      vkGetDeviceProcAddr(vk_device, "vkCreateShaderModule"));
  if (!vkCreateShaderModuleFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkCreateShaderModule";
    return false;
  }

  vkDestroyBufferFn_ = reinterpret_cast<PFN_vkDestroyBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyBuffer"));
  if (!vkDestroyBufferFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyBuffer";
    return false;
  }

  vkDestroyCommandPoolFn_ = reinterpret_cast<PFN_vkDestroyCommandPool>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyCommandPool"));
  if (!vkDestroyCommandPoolFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyCommandPool";
    return false;
  }

  vkDestroyDescriptorPoolFn_ = reinterpret_cast<PFN_vkDestroyDescriptorPool>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyDescriptorPool"));
  if (!vkDestroyDescriptorPoolFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDescriptorPool";
    return false;
  }

  vkDestroyDescriptorSetLayoutFn_ =
      reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(
          vkGetDeviceProcAddr(vk_device, "vkDestroyDescriptorSetLayout"));
  if (!vkDestroyDescriptorSetLayoutFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDescriptorSetLayout";
    return false;
  }

  vkDestroyDeviceFn_ = reinterpret_cast<PFN_vkDestroyDevice>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyDevice"));
  if (!vkDestroyDeviceFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyDevice";
    return false;
  }

  vkDestroyFenceFn_ = reinterpret_cast<PFN_vkDestroyFence>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyFence"));
  if (!vkDestroyFenceFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyFence";
    return false;
  }

  vkDestroyFramebufferFn_ = reinterpret_cast<PFN_vkDestroyFramebuffer>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyFramebuffer"));
  if (!vkDestroyFramebufferFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyFramebuffer";
    return false;
  }

  vkDestroyImageFn_ = reinterpret_cast<PFN_vkDestroyImage>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyImage"));
  if (!vkDestroyImageFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyImage";
    return false;
  }

  vkDestroyImageViewFn_ = reinterpret_cast<PFN_vkDestroyImageView>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyImageView"));
  if (!vkDestroyImageViewFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyImageView";
    return false;
  }

  vkDestroyRenderPassFn_ = reinterpret_cast<PFN_vkDestroyRenderPass>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyRenderPass"));
  if (!vkDestroyRenderPassFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyRenderPass";
    return false;
  }

  vkDestroySamplerFn_ = reinterpret_cast<PFN_vkDestroySampler>(
      vkGetDeviceProcAddr(vk_device, "vkDestroySampler"));
  if (!vkDestroySamplerFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroySampler";
    return false;
  }

  vkDestroySemaphoreFn_ = reinterpret_cast<PFN_vkDestroySemaphore>(
      vkGetDeviceProcAddr(vk_device, "vkDestroySemaphore"));
  if (!vkDestroySemaphoreFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroySemaphore";
    return false;
  }

  vkDestroyShaderModuleFn_ = reinterpret_cast<PFN_vkDestroyShaderModule>(
      vkGetDeviceProcAddr(vk_device, "vkDestroyShaderModule"));
  if (!vkDestroyShaderModuleFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDestroyShaderModule";
    return false;
  }

  vkDeviceWaitIdleFn_ = reinterpret_cast<PFN_vkDeviceWaitIdle>(
      vkGetDeviceProcAddr(vk_device, "vkDeviceWaitIdle"));
  if (!vkDeviceWaitIdleFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkDeviceWaitIdle";
    return false;
  }

  vkEndCommandBufferFn_ = reinterpret_cast<PFN_vkEndCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkEndCommandBuffer"));
  if (!vkEndCommandBufferFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkEndCommandBuffer";
    return false;
  }

  vkFreeCommandBuffersFn_ = reinterpret_cast<PFN_vkFreeCommandBuffers>(
      vkGetDeviceProcAddr(vk_device, "vkFreeCommandBuffers"));
  if (!vkFreeCommandBuffersFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeCommandBuffers";
    return false;
  }

  vkFreeDescriptorSetsFn_ = reinterpret_cast<PFN_vkFreeDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkFreeDescriptorSets"));
  if (!vkFreeDescriptorSetsFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeDescriptorSets";
    return false;
  }

  vkFreeMemoryFn_ = reinterpret_cast<PFN_vkFreeMemory>(
      vkGetDeviceProcAddr(vk_device, "vkFreeMemory"));
  if (!vkFreeMemoryFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkFreeMemory";
    return false;
  }

  vkGetBufferMemoryRequirementsFn_ =
      reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(
          vkGetDeviceProcAddr(vk_device, "vkGetBufferMemoryRequirements"));
  if (!vkGetBufferMemoryRequirementsFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetBufferMemoryRequirements";
    return false;
  }

  vkGetDeviceQueueFn_ = reinterpret_cast<PFN_vkGetDeviceQueue>(
      vkGetDeviceProcAddr(vk_device, "vkGetDeviceQueue"));
  if (!vkGetDeviceQueueFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetDeviceQueue";
    return false;
  }

  vkGetFenceStatusFn_ = reinterpret_cast<PFN_vkGetFenceStatus>(
      vkGetDeviceProcAddr(vk_device, "vkGetFenceStatus"));
  if (!vkGetFenceStatusFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetFenceStatus";
    return false;
  }

  vkGetImageMemoryRequirementsFn_ =
      reinterpret_cast<PFN_vkGetImageMemoryRequirements>(
          vkGetDeviceProcAddr(vk_device, "vkGetImageMemoryRequirements"));
  if (!vkGetImageMemoryRequirementsFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkGetImageMemoryRequirements";
    return false;
  }

  vkMapMemoryFn_ = reinterpret_cast<PFN_vkMapMemory>(
      vkGetDeviceProcAddr(vk_device, "vkMapMemory"));
  if (!vkMapMemoryFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkMapMemory";
    return false;
  }

  vkQueueSubmitFn_ = reinterpret_cast<PFN_vkQueueSubmit>(
      vkGetDeviceProcAddr(vk_device, "vkQueueSubmit"));
  if (!vkQueueSubmitFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkQueueSubmit";
    return false;
  }

  vkQueueWaitIdleFn_ = reinterpret_cast<PFN_vkQueueWaitIdle>(
      vkGetDeviceProcAddr(vk_device, "vkQueueWaitIdle"));
  if (!vkQueueWaitIdleFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkQueueWaitIdle";
    return false;
  }

  vkResetCommandBufferFn_ = reinterpret_cast<PFN_vkResetCommandBuffer>(
      vkGetDeviceProcAddr(vk_device, "vkResetCommandBuffer"));
  if (!vkResetCommandBufferFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkResetCommandBuffer";
    return false;
  }

  vkResetFencesFn_ = reinterpret_cast<PFN_vkResetFences>(
      vkGetDeviceProcAddr(vk_device, "vkResetFences"));
  if (!vkResetFencesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkResetFences";
    return false;
  }

  vkUnmapMemoryFn_ = reinterpret_cast<PFN_vkUnmapMemory>(
      vkGetDeviceProcAddr(vk_device, "vkUnmapMemory"));
  if (!vkUnmapMemoryFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkUnmapMemory";
    return false;
  }

  vkUpdateDescriptorSetsFn_ = reinterpret_cast<PFN_vkUpdateDescriptorSets>(
      vkGetDeviceProcAddr(vk_device, "vkUpdateDescriptorSets"));
  if (!vkUpdateDescriptorSetsFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkUpdateDescriptorSets";
    return false;
  }

  vkWaitForFencesFn_ = reinterpret_cast<PFN_vkWaitForFences>(
      vkGetDeviceProcAddr(vk_device, "vkWaitForFences"));
  if (!vkWaitForFencesFn_) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                  << "vkWaitForFences";
    return false;
  }

  if (api_version >= VK_API_VERSION_1_1) {
    vkGetDeviceQueue2Fn_ = reinterpret_cast<PFN_vkGetDeviceQueue2>(
        vkGetDeviceProcAddr(vk_device, "vkGetDeviceQueue2"));
    if (!vkGetDeviceQueue2Fn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetDeviceQueue2";
      return false;
    }

    vkGetImageMemoryRequirements2Fn_ =
        reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(
            vkGetDeviceProcAddr(vk_device, "vkGetImageMemoryRequirements2"));
    if (!vkGetImageMemoryRequirements2Fn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetImageMemoryRequirements2";
      return false;
    }
  }

#if defined(OS_ANDROID)
  if (gfx::HasExtension(
          enabled_extensions,
          VK_ANDROID_EXTERNAL_MEMORY_ANDROID_HARDWARE_BUFFER_EXTENSION_NAME)) {
    vkGetAndroidHardwareBufferPropertiesANDROIDFn_ =
        reinterpret_cast<PFN_vkGetAndroidHardwareBufferPropertiesANDROID>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetAndroidHardwareBufferPropertiesANDROID"));
    if (!vkGetAndroidHardwareBufferPropertiesANDROIDFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetAndroidHardwareBufferPropertiesANDROID";
      return false;
    }
  }
#endif  // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME)) {
    vkGetSemaphoreFdKHRFn_ = reinterpret_cast<PFN_vkGetSemaphoreFdKHR>(
        vkGetDeviceProcAddr(vk_device, "vkGetSemaphoreFdKHR"));
    if (!vkGetSemaphoreFdKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSemaphoreFdKHR";
      return false;
    }

    vkImportSemaphoreFdKHRFn_ = reinterpret_cast<PFN_vkImportSemaphoreFdKHR>(
        vkGetDeviceProcAddr(vk_device, "vkImportSemaphoreFdKHR"));
    if (!vkImportSemaphoreFdKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkImportSemaphoreFdKHR";
      return false;
    }
  }
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (gfx::HasExtension(enabled_extensions,
                        VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME)) {
    vkGetMemoryFdKHRFn_ = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
        vkGetDeviceProcAddr(vk_device, "vkGetMemoryFdKHR"));
    if (!vkGetMemoryFdKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryFdKHR";
      return false;
    }

    vkGetMemoryFdPropertiesKHRFn_ =
        reinterpret_cast<PFN_vkGetMemoryFdPropertiesKHR>(
            vkGetDeviceProcAddr(vk_device, "vkGetMemoryFdPropertiesKHR"));
    if (!vkGetMemoryFdPropertiesKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryFdPropertiesKHR";
      return false;
    }
  }
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME)) {
    vkImportSemaphoreZirconHandleFUCHSIAFn_ =
        reinterpret_cast<PFN_vkImportSemaphoreZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkImportSemaphoreZirconHandleFUCHSIA"));
    if (!vkImportSemaphoreZirconHandleFUCHSIAFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkImportSemaphoreZirconHandleFUCHSIA";
      return false;
    }

    vkGetSemaphoreZirconHandleFUCHSIAFn_ =
        reinterpret_cast<PFN_vkGetSemaphoreZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetSemaphoreZirconHandleFUCHSIA"));
    if (!vkGetSemaphoreZirconHandleFUCHSIAFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSemaphoreZirconHandleFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME)) {
    vkGetMemoryZirconHandleFUCHSIAFn_ =
        reinterpret_cast<PFN_vkGetMemoryZirconHandleFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, "vkGetMemoryZirconHandleFUCHSIA"));
    if (!vkGetMemoryZirconHandleFUCHSIAFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetMemoryZirconHandleFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

#if defined(OS_FUCHSIA)
  if (gfx::HasExtension(enabled_extensions,
                        VK_FUCHSIA_BUFFER_COLLECTION_EXTENSION_NAME)) {
    vkCreateBufferCollectionFUCHSIAFn_ =
        reinterpret_cast<PFN_vkCreateBufferCollectionFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, "vkCreateBufferCollectionFUCHSIA"));
    if (!vkCreateBufferCollectionFUCHSIAFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateBufferCollectionFUCHSIA";
      return false;
    }

    vkSetBufferCollectionConstraintsFUCHSIAFn_ =
        reinterpret_cast<PFN_vkSetBufferCollectionConstraintsFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkSetBufferCollectionConstraintsFUCHSIA"));
    if (!vkSetBufferCollectionConstraintsFUCHSIAFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkSetBufferCollectionConstraintsFUCHSIA";
      return false;
    }

    vkGetBufferCollectionPropertiesFUCHSIAFn_ =
        reinterpret_cast<PFN_vkGetBufferCollectionPropertiesFUCHSIA>(
            vkGetDeviceProcAddr(vk_device,
                                "vkGetBufferCollectionPropertiesFUCHSIA"));
    if (!vkGetBufferCollectionPropertiesFUCHSIAFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetBufferCollectionPropertiesFUCHSIA";
      return false;
    }

    vkDestroyBufferCollectionFUCHSIAFn_ =
        reinterpret_cast<PFN_vkDestroyBufferCollectionFUCHSIA>(
            vkGetDeviceProcAddr(vk_device, "vkDestroyBufferCollectionFUCHSIA"));
    if (!vkDestroyBufferCollectionFUCHSIAFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroyBufferCollectionFUCHSIA";
      return false;
    }
  }
#endif  // defined(OS_FUCHSIA)

  if (gfx::HasExtension(enabled_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
    vkAcquireNextImageKHRFn_ = reinterpret_cast<PFN_vkAcquireNextImageKHR>(
        vkGetDeviceProcAddr(vk_device, "vkAcquireNextImageKHR"));
    if (!vkAcquireNextImageKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkAcquireNextImageKHR";
      return false;
    }

    vkCreateSwapchainKHRFn_ = reinterpret_cast<PFN_vkCreateSwapchainKHR>(
        vkGetDeviceProcAddr(vk_device, "vkCreateSwapchainKHR"));
    if (!vkCreateSwapchainKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkCreateSwapchainKHR";
      return false;
    }

    vkDestroySwapchainKHRFn_ = reinterpret_cast<PFN_vkDestroySwapchainKHR>(
        vkGetDeviceProcAddr(vk_device, "vkDestroySwapchainKHR"));
    if (!vkDestroySwapchainKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkDestroySwapchainKHR";
      return false;
    }

    vkGetSwapchainImagesKHRFn_ = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(
        vkGetDeviceProcAddr(vk_device, "vkGetSwapchainImagesKHR"));
    if (!vkGetSwapchainImagesKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkGetSwapchainImagesKHR";
      return false;
    }

    vkQueuePresentKHRFn_ = reinterpret_cast<PFN_vkQueuePresentKHR>(
        vkGetDeviceProcAddr(vk_device, "vkQueuePresentKHR"));
    if (!vkQueuePresentKHRFn_) {
      DLOG(WARNING) << "Failed to bind vulkan entrypoint: "
                    << "vkQueuePresentKHR";
      return false;
    }
  }

  return true;
}

}  // namespace gpu
