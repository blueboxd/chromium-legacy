// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********
//     See loader_source_generator.py for modifications
// ************************************************************

// Copyright (c) 2017-2019 The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Mark Young <marky@lunarg.com>
//

#pragma once
#include <unordered_map>
#include <thread>
#include <mutex>

#include "loader_interfaces.h"


#ifdef __cplusplus
extern "C" { 
#endif

// Loader manually generated function prototypes


// ---- Core 0.90 loader manual functions
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(
    XrInstance                                  instance,
    const char*                                 name,
    PFN_xrVoidFunction*                         function);
XRAPI_ATTR XrResult XRAPI_CALL LoaderXrTermGetInstanceProcAddr(
    XrInstance                                  instance,
    const char*                                 name,
    PFN_xrVoidFunction*                         function);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateApiLayerProperties(
    uint32_t                                    propertyCapacityInput,
    uint32_t*                                   propertyCountOutput,
    XrApiLayerProperties*                       properties);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateInstanceExtensionProperties(
    const char*                                 layerName,
    uint32_t                                    propertyCapacityInput,
    uint32_t*                                   propertyCountOutput,
    XrExtensionProperties*                      properties);
XRAPI_ATTR XrResult XRAPI_CALL xrCreateInstance(
    const XrInstanceCreateInfo*                 createInfo,
    XrInstance*                                 instance);
XRAPI_ATTR XrResult XRAPI_CALL LoaderXrTermCreateInstance(
    const XrInstanceCreateInfo*                 createInfo,
    XrInstance*                                 instance);
XRAPI_ATTR XrResult XRAPI_CALL xrDestroyInstance(
    XrInstance                                  instance);
XRAPI_ATTR XrResult XRAPI_CALL LoaderXrTermDestroyInstance(
    XrInstance                                  instance);
XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProperties(
    XrInstance                                  instance,
    XrInstanceProperties*                       instanceProperties);
XRAPI_ATTR XrResult XRAPI_CALL xrPollEvent(
    XrInstance                                  instance,
    XrEventDataBuffer*                          eventData);
XRAPI_ATTR XrResult XRAPI_CALL xrResultToString(
    XrInstance                                  instance,
    XrResult                                    value,
    char                                        buffer[XR_MAX_RESULT_STRING_SIZE]);
XRAPI_ATTR XrResult XRAPI_CALL xrStructureTypeToString(
    XrInstance                                  instance,
    XrStructureType                             value,
    char                                        buffer[XR_MAX_STRUCTURE_NAME_SIZE]);
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystem(
    XrInstance                                  instance,
    const XrSystemGetInfo*                      getInfo,
    XrSystemId*                                 systemId);
XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrSystemProperties*                         properties);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateEnvironmentBlendModes(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    uint32_t                                    environmentBlendModeCapacityInput,
    uint32_t*                                   environmentBlendModeCountOutput,
    XrEnvironmentBlendMode*                     environmentBlendModes);
XRAPI_ATTR XrResult XRAPI_CALL xrCreateSession(
    XrInstance                                  instance,
    const XrSessionCreateInfo*                  createInfo,
    XrSession*                                  session);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurations(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    uint32_t                                    viewConfigurationTypeCapacityInput,
    uint32_t*                                   viewConfigurationTypeCountOutput,
    XrViewConfigurationType*                    viewConfigurationTypes);
XRAPI_ATTR XrResult XRAPI_CALL xrGetViewConfigurationProperties(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrViewConfigurationType                     viewConfigurationType,
    XrViewConfigurationProperties*              configurationProperties);
XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateViewConfigurationViews(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrViewConfigurationType                     viewConfigurationType,
    uint32_t                                    viewCapacityInput,
    uint32_t*                                   viewCountOutput,
    XrViewConfigurationView*                    views);
XRAPI_ATTR XrResult XRAPI_CALL xrStringToPath(
    XrInstance                                  instance,
    const char*                                 pathString,
    XrPath*                                     path);
XRAPI_ATTR XrResult XRAPI_CALL xrPathToString(
    XrInstance                                  instance,
    XrPath                                      path,
    uint32_t                                    bufferCapacityInput,
    uint32_t*                                   bufferCountOutput,
    char*                                       buffer);

// ---- XR_KHR_opengl_enable loader manual functions
#if defined(XR_USE_GRAPHICS_API_OPENGL)
XRAPI_ATTR XrResult XRAPI_CALL xrGetOpenGLGraphicsRequirementsKHR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrGraphicsRequirementsOpenGLKHR*            graphicsRequirements);
#endif // defined(XR_USE_GRAPHICS_API_OPENGL)

// ---- XR_KHR_opengl_es_enable loader manual functions
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
XRAPI_ATTR XrResult XRAPI_CALL xrGetOpenGLESGraphicsRequirementsKHR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrGraphicsRequirementsOpenGLESKHR*          graphicsRequirements);
#endif // defined(XR_USE_GRAPHICS_API_OPENGL_ES)

// ---- XR_KHR_vulkan_enable loader manual functions
#if defined(XR_USE_GRAPHICS_API_VULKAN)
XRAPI_ATTR XrResult XRAPI_CALL xrGetVulkanInstanceExtensionsKHR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    uint32_t                                    namesCapacityInput,
    uint32_t*                                   namesCountOutput,
    char*                                       namesString);
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
XRAPI_ATTR XrResult XRAPI_CALL xrGetVulkanDeviceExtensionsKHR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    uint32_t                                    namesCapacityInput,
    uint32_t*                                   namesCountOutput,
    char*                                       namesString);
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
XRAPI_ATTR XrResult XRAPI_CALL xrGetVulkanGraphicsDeviceKHR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    VkInstance                                  vkInstance,
    VkPhysicalDevice*                           vkPhysicalDevice);
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
XRAPI_ATTR XrResult XRAPI_CALL xrGetVulkanGraphicsRequirementsKHR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrGraphicsRequirementsVulkanKHR*            graphicsRequirements);
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)

// ---- XR_KHR_D3D10_enable loader manual functions
#if defined(XR_USE_GRAPHICS_API_D3D10)
XRAPI_ATTR XrResult XRAPI_CALL xrGetD3D10GraphicsRequirementsKHR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrGraphicsRequirementsD3D10KHR*             graphicsRequirements);
#endif // defined(XR_USE_GRAPHICS_API_D3D10)

// ---- XR_KHR_D3D11_enable loader manual functions
#if defined(XR_USE_GRAPHICS_API_D3D11)
XRAPI_ATTR XrResult XRAPI_CALL xrGetD3D11GraphicsRequirementsKHR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrGraphicsRequirementsD3D11KHR*             graphicsRequirements);
#endif // defined(XR_USE_GRAPHICS_API_D3D11)

// ---- XR_KHR_D3D12_enable loader manual functions
#if defined(XR_USE_GRAPHICS_API_D3D12)
XRAPI_ATTR XrResult XRAPI_CALL xrGetD3D12GraphicsRequirementsKHR(
    XrInstance                                  instance,
    XrSystemId                                  systemId,
    XrGraphicsRequirementsD3D12KHR*             graphicsRequirements);
#endif // defined(XR_USE_GRAPHICS_API_D3D12)

// ---- XR_KHR_win32_convert_performance_counter_time loader manual functions
#if defined(XR_USE_PLATFORM_WIN32)
XRAPI_ATTR XrResult XRAPI_CALL xrConvertWin32PerformanceCounterToTimeKHR(
    XrInstance                                  instance,
    const LARGE_INTEGER*                        performanceCounter,
    XrTime*                                     time);
#endif // defined(XR_USE_PLATFORM_WIN32)
#if defined(XR_USE_PLATFORM_WIN32)
XRAPI_ATTR XrResult XRAPI_CALL xrConvertTimeToWin32PerformanceCounterKHR(
    XrInstance                                  instance,
    XrTime                                      time,
    LARGE_INTEGER*                              performanceCounter);
#endif // defined(XR_USE_PLATFORM_WIN32)

// ---- XR_KHR_convert_timespec_time loader manual functions
#if defined(XR_USE_TIMESPEC)
XRAPI_ATTR XrResult XRAPI_CALL xrConvertTimespecTimeToTimeKHR(
    XrInstance                                  instance,
    const struct timespec*                      timespecTime,
    XrTime*                                     time);
#endif // defined(XR_USE_TIMESPEC)
#if defined(XR_USE_TIMESPEC)
XRAPI_ATTR XrResult XRAPI_CALL xrConvertTimeToTimespecTimeKHR(
    XrInstance                                  instance,
    XrTime                                      time,
    struct timespec*                            timespecTime);
#endif // defined(XR_USE_TIMESPEC)

// ---- XR_EXT_debug_utils loader manual functions
XRAPI_ATTR XrResult XRAPI_CALL xrSetDebugUtilsObjectNameEXT(
    XrInstance                                  instance,
    const XrDebugUtilsObjectNameInfoEXT*        nameInfo);
XRAPI_ATTR XrResult XRAPI_CALL LoaderXrTermSetDebugUtilsObjectNameEXT(
    XrInstance                                  instance,
    const XrDebugUtilsObjectNameInfoEXT*        nameInfo);
XRAPI_ATTR XrResult XRAPI_CALL xrCreateDebugUtilsMessengerEXT(
    XrInstance                                  instance,
    const XrDebugUtilsMessengerCreateInfoEXT*   createInfo,
    XrDebugUtilsMessengerEXT*                   messenger);
XRAPI_ATTR XrResult XRAPI_CALL LoaderXrTermCreateDebugUtilsMessengerEXT(
    XrInstance                                  instance,
    const XrDebugUtilsMessengerCreateInfoEXT*   createInfo,
    XrDebugUtilsMessengerEXT*                   messenger);
XRAPI_ATTR XrResult XRAPI_CALL xrDestroyDebugUtilsMessengerEXT(
    XrDebugUtilsMessengerEXT                    messenger);
XRAPI_ATTR XrResult XRAPI_CALL LoaderXrTermDestroyDebugUtilsMessengerEXT(
    XrDebugUtilsMessengerEXT                    messenger);
XRAPI_ATTR XrResult                                    XRAPI_CALL xrSubmitDebugUtilsMessageEXT(
    XrInstance                                  instance,
    XrDebugUtilsMessageSeverityFlagsEXT         messageSeverity,
    XrDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const XrDebugUtilsMessengerCallbackDataEXT* callbackData);
XRAPI_ATTR XrResult                                    XRAPI_CALL LoaderXrTermSubmitDebugUtilsMessageEXT(
    XrInstance                                  instance,
    XrDebugUtilsMessageSeverityFlagsEXT         messageSeverity,
    XrDebugUtilsMessageTypeFlagsEXT             messageTypes,
    const XrDebugUtilsMessengerCallbackDataEXT* callbackData);
XRAPI_ATTR XrResult XRAPI_CALL xrSessionBeginDebugUtilsLabelRegionEXT(
    XrSession                                   session,
    const XrDebugUtilsLabelEXT*                 labelInfo);
XRAPI_ATTR XrResult XRAPI_CALL xrSessionEndDebugUtilsLabelRegionEXT(
    XrSession                                   session);
XRAPI_ATTR XrResult XRAPI_CALL xrSessionInsertDebugUtilsLabelEXT(
    XrSession                                   session,
    const XrDebugUtilsLabelEXT*                 labelInfo);

// Special use function to handle creating API Layer information during xrCreateInstance
XRAPI_ATTR XrResult XRAPI_CALL LoaderXrTermCreateApiLayerInstance(const XrInstanceCreateInfo* info,
                                                                  const struct XrApiLayerCreateInfo* apiLayerInfo,
                                                                  XrInstance* instance);


// Generated loader terminator prototypes
XRAPI_ATTR XrResult XRAPI_CALL LoaderGenTermXrResultToString(
    XrInstance                                  instance,
    XrResult                                    value,
    char                                        buffer[XR_MAX_RESULT_STRING_SIZE]);
XRAPI_ATTR XrResult XRAPI_CALL LoaderGenTermXrStructureTypeToString(
    XrInstance                                  instance,
    XrStructureType                             value,
    char                                        buffer[XR_MAX_STRUCTURE_NAME_SIZE]);
// Instance Init Dispatch Table (put all terminators in first)
void LoaderGenInitInstanceDispatchTable(XrInstance runtime_instance,
                                        std::unique_ptr<XrGeneratedDispatchTable>& table);

#ifdef __cplusplus
} // extern "C"
#endif

// Unordered maps and mutexes to lookup the instance for a given object type
extern std::unordered_map<XrInstance, class LoaderInstance*> g_instance_map;
extern std::mutex g_instance_mutex;
extern std::unordered_map<XrSession, class LoaderInstance*> g_session_map;
extern std::mutex g_session_mutex;
extern std::unordered_map<XrSpace, class LoaderInstance*> g_space_map;
extern std::mutex g_space_mutex;
extern std::unordered_map<XrAction, class LoaderInstance*> g_action_map;
extern std::mutex g_action_mutex;
extern std::unordered_map<XrSwapchain, class LoaderInstance*> g_swapchain_map;
extern std::mutex g_swapchain_mutex;
extern std::unordered_map<XrActionSet, class LoaderInstance*> g_actionset_map;
extern std::mutex g_actionset_mutex;
extern std::unordered_map<XrDebugUtilsMessengerEXT, class LoaderInstance*> g_debugutilsmessengerext_map;
extern std::mutex g_debugutilsmessengerext_mutex;

// Function used to clean up any residual map values that point to an instance prior to that
// instance being deleted.
void LoaderCleanUpMapsForInstance(class LoaderInstance *instance);


