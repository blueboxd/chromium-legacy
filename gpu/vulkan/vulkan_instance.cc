// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_instance.h"

#include <vector>

#include "base/logging.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

namespace gpu {

VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanErrorCallback(VkDebugReportFlagsEXT flags,
                    VkDebugReportObjectTypeEXT objectType,
                    uint64_t object,
                    size_t location,
                    int32_t messageCode,
                    const char* pLayerPrefix,
                    const char* pMessage,
                    void* pUserData) {
  LOG(ERROR) << pMessage;
  return VK_TRUE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanWarningCallback(VkDebugReportFlagsEXT flags,
                      VkDebugReportObjectTypeEXT objectType,
                      uint64_t object,
                      size_t location,
                      int32_t messageCode,
                      const char* pLayerPrefix,
                      const char* pMessage,
                      void* pUserData) {
  LOG(WARNING) << pMessage;
  return VK_TRUE;
}

VulkanInstance::VulkanInstance() {}

VulkanInstance::~VulkanInstance() {
  Destroy();
}

bool VulkanInstance::Initialize(
    const std::vector<const char*>& required_extensions,
    const std::vector<const char*>& required_layers) {
  DCHECK(!vk_instance_);

  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();

  if (!vulkan_function_pointers->BindUnassociatedFunctionPointers())
    return false;

  uint32_t supported_api_version = VK_MAKE_VERSION(1, 0, 0);
  if (vulkan_function_pointers->vkEnumerateInstanceVersionFn) {
    vulkan_function_pointers->vkEnumerateInstanceVersionFn(
        &supported_api_version);
  }

#if defined(OS_ANDROID)
  // Ensure that android works only with vulkan apiVersion >= 1.1. Vulkan will
  // only be enabled for Android P+ and Android P+ requires vulkan
  // apiVersion >= 1.1.
  if (supported_api_version < VK_MAKE_VERSION(1, 1, 0))
    return false;
#endif

  // Use Vulkan 1.1 if it's available.
  api_version_ = (supported_api_version >= VK_MAKE_VERSION(1, 1, 0))
                     ? VK_MAKE_VERSION(1, 1, 0)
                     : VK_MAKE_VERSION(1, 0, 0);

  VkResult result = VK_SUCCESS;

  VkApplicationInfo app_info = {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Chromium";
  app_info.apiVersion = api_version_;

  std::vector<const char*> enabled_extensions = required_extensions;

  uint32_t num_instance_exts = 0;
  result = vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_exts,
                                                  nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceExtensionProperties(NULL) failed: "
                << result;
    return false;
  }

  std::vector<VkExtensionProperties> instance_exts(num_instance_exts);
  result = vkEnumerateInstanceExtensionProperties(nullptr, &num_instance_exts,
                                                  instance_exts.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceExtensionProperties() failed: "
                << result;
    return false;
  }

  for (const VkExtensionProperties& ext_property : instance_exts) {
    if (strcmp(ext_property.extensionName,
               VK_EXT_DEBUG_REPORT_EXTENSION_NAME) == 0) {
      debug_report_enabled_ = true;
      enabled_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
    }
  }

#if DCHECK_IS_ON()
  for (const char* enabled_extension : enabled_extensions) {
    bool found = false;
    for (const VkExtensionProperties& ext_property : instance_exts) {
      if (strcmp(ext_property.extensionName, enabled_extension) == 0) {
        found = true;
        break;
      }
    }
    if (!found) {
      DLOG(ERROR) << "Required extension " << enabled_extension
                  << " missing from enumerated Vulkan extensions. "
                     "vkCreateInstance will likely fail.";
    }
  }
#endif

  std::vector<const char*> enabled_layer_names = required_layers;
#if DCHECK_IS_ON()
  uint32_t num_instance_layers = 0;
  result = vkEnumerateInstanceLayerProperties(&num_instance_layers, nullptr);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceLayerProperties(NULL) failed: "
                << result;
    return false;
  }

  std::vector<VkLayerProperties> instance_layers(num_instance_layers);
  result = vkEnumerateInstanceLayerProperties(&num_instance_layers,
                                              instance_layers.data());
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkEnumerateInstanceLayerProperties() failed: " << result;
    return false;
  }

  // TODO(crbug.com/843346): Make validation work in combination with
  // VK_KHR_xlib_surface or switch to VK_KHR_xcb_surface.
  constexpr base::StringPiece xlib_surface_extension_name(
      "VK_KHR_xlib_surface");
  bool require_xlib_surface_extension =
      std::find_if(enabled_extensions.begin(), enabled_extensions.end(),
                   [xlib_surface_extension_name](const char* e) {
                     return xlib_surface_extension_name == e;
                   }) != enabled_extensions.end();

  // VK_LAYER_LUNARG_standard_validation 1.1.106 is required to support
  // VK_KHR_xlib_surface.
  constexpr base::StringPiece standard_validation(
      "VK_LAYER_LUNARG_standard_validation");
  for (const VkLayerProperties& layer_property : instance_layers) {
    if (standard_validation != layer_property.layerName)
      continue;
    if (!require_xlib_surface_extension ||
        layer_property.specVersion >= VK_MAKE_VERSION(1, 1, 106)) {
      enabled_layer_names.push_back(standard_validation.data());
    }
    break;
  }
#endif  // DCHECK_IS_ON()

  VkInstanceCreateInfo instance_create_info = {
      VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,  // sType
      nullptr,                                 // pNext
      0,                                       // flags
      &app_info,                               // pApplicationInfo
      enabled_layer_names.size(),              // enableLayerCount
      enabled_layer_names.data(),              // ppEnabledLayerNames
      enabled_extensions.size(),               // enabledExtensionCount
      enabled_extensions.data(),               // ppEnabledExtensionNames
  };

  result = vkCreateInstance(&instance_create_info, nullptr, &vk_instance_);
  if (VK_SUCCESS != result) {
    DLOG(ERROR) << "vkCreateInstance() failed: " << result;
    return false;
  }

  enabled_extensions_ = gfx::ExtensionSet(std::begin(enabled_extensions),
                                          std::end(enabled_extensions));

#if DCHECK_IS_ON()
  // Register our error logging function.
  if (debug_report_enabled_) {
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance_,
                                  "vkCreateDebugReportCallbackEXT"));
    DCHECK(vkCreateDebugReportCallbackEXT);

    VkDebugReportCallbackCreateInfoEXT cb_create_info = {};
    cb_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;

    cb_create_info.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT;
    cb_create_info.pfnCallback = &VulkanErrorCallback;
    result = vkCreateDebugReportCallbackEXT(vk_instance_, &cb_create_info,
                                            nullptr, &error_callback_);
    if (VK_SUCCESS != result) {
      error_callback_ = VK_NULL_HANDLE;
      DLOG(ERROR) << "vkCreateDebugReportCallbackEXT(ERROR) failed: " << result;
      return false;
    }

    cb_create_info.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
                           VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    cb_create_info.pfnCallback = &VulkanWarningCallback;
    result = vkCreateDebugReportCallbackEXT(vk_instance_, &cb_create_info,
                                            nullptr, &warning_callback_);
    if (VK_SUCCESS != result) {
      warning_callback_ = VK_NULL_HANDLE;
      DLOG(ERROR) << "vkCreateDebugReportCallbackEXT(WARN) failed: " << result;
      return false;
    }
  }
#endif

  return vulkan_function_pointers->BindInstanceFunctionPointers(
      vk_instance_, api_version_, enabled_extensions_);
}

void VulkanInstance::Destroy() {
#if DCHECK_IS_ON()
  if (debug_report_enabled_ && (error_callback_ != VK_NULL_HANDLE ||
                                warning_callback_ != VK_NULL_HANDLE)) {
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT =
        reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(
            vkGetInstanceProcAddr(vk_instance_,
                                  "vkDestroyDebugReportCallbackEXT"));
    DCHECK(vkDestroyDebugReportCallbackEXT);
    if (error_callback_ != VK_NULL_HANDLE) {
      vkDestroyDebugReportCallbackEXT(vk_instance_, error_callback_, nullptr);
      error_callback_ = VK_NULL_HANDLE;
    }
    if (warning_callback_ != VK_NULL_HANDLE) {
      vkDestroyDebugReportCallbackEXT(vk_instance_, warning_callback_, nullptr);
      warning_callback_ = VK_NULL_HANDLE;
    }
  }
#endif
  if (vk_instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(vk_instance_, nullptr);
    vk_instance_ = VK_NULL_HANDLE;
  }
  VulkanFunctionPointers* vulkan_function_pointers =
      gpu::GetVulkanFunctionPointers();
  if (vulkan_function_pointers->vulkan_loader_library_)
    base::UnloadNativeLibrary(vulkan_function_pointers->vulkan_loader_library_);
  vulkan_function_pointers->vulkan_loader_library_ = nullptr;
}

}  // namespace gpu
