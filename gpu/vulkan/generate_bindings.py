#!/usr/bin/env python
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""code generator for Vulkan function pointers."""

import filecmp
import optparse
import os
import platform
import sys
from string import Template
from subprocess import call

VULKAN_UNASSOCIATED_FUNCTIONS = [
# vkGetInstanceProcAddr belongs here but is handled specially.
# vkEnumerateInstanceVersion belongs here but is handled specially.
{ 'name': 'vkCreateInstance' },
{ 'name': 'vkEnumerateInstanceExtensionProperties' },
{ 'name': 'vkEnumerateInstanceLayerProperties' },
]

VULKAN_INSTANCE_FUNCTIONS = [
{ 'name': 'vkDestroyInstance' },
# vkDestroySurfaceKHR belongs here but is handled specially.
{ 'name': 'vkEnumeratePhysicalDevices' },
{ 'name': 'vkGetDeviceProcAddr' },
]

VULKAN_PHYSICAL_DEVICE_FUNCTIONS = [
{ 'name': 'vkCreateDevice' },
{ 'name': 'vkEnumerateDeviceLayerProperties' },
{ 'name': 'vkGetPhysicalDeviceFeatures' },
{ 'name': 'vkGetPhysicalDeviceMemoryProperties'},
{ 'name': 'vkGetPhysicalDeviceQueueFamilyProperties' },
{ 'name': 'vkGetPhysicalDeviceProperties' },
# The following functions belong here but are handled specially:
# vkGetPhysicalDeviceSurfaceCapabilitiesKHR
# vkGetPhysicalDeviceSurfaceFormatsKHR
# vkGetPhysicalDeviceSurfaceSupportKHR
# vkGetPhysicalDeviceXlibPresentationSupportKHR
]

VULKAN_DEVICE_FUNCTIONS = [
{ 'name': 'vkAllocateCommandBuffers' },
{ 'name': 'vkAllocateDescriptorSets' },
{ 'name': 'vkAllocateMemory' },
{ 'name': 'vkBindBufferMemory' },
{ 'name': 'vkBindImageMemory' },
{ 'name': 'vkCreateCommandPool' },
{ 'name': 'vkCreateBuffer' },
{ 'name': 'vkCreateDescriptorPool' },
{ 'name': 'vkCreateDescriptorSetLayout' },
{ 'name': 'vkCreateFence' },
{ 'name': 'vkCreateFramebuffer' },
{ 'name': 'vkCreateImage' },
{ 'name': 'vkCreateImageView' },
{ 'name': 'vkCreateRenderPass' },
{ 'name': 'vkCreateSampler' },
{ 'name': 'vkCreateSemaphore' },
{ 'name': 'vkCreateShaderModule' },
{ 'name': 'vkDestroyBuffer' },
{ 'name': 'vkDestroyCommandPool' },
{ 'name': 'vkDestroyDescriptorPool' },
{ 'name': 'vkDestroyDescriptorSetLayout' },
{ 'name': 'vkDestroyDevice' },
{ 'name': 'vkDestroyFramebuffer' },
{ 'name': 'vkDestroyFence' },
{ 'name': 'vkDestroyImage' },
{ 'name': 'vkDestroyImageView' },
{ 'name': 'vkDestroyRenderPass' },
{ 'name': 'vkDestroySampler' },
{ 'name': 'vkDestroySemaphore' },
{ 'name': 'vkDestroyShaderModule' },
{ 'name': 'vkDeviceWaitIdle' },
{ 'name': 'vkFreeCommandBuffers' },
{ 'name': 'vkFreeDescriptorSets' },
{ 'name': 'vkFreeMemory' },
{ 'name': 'vkGetBufferMemoryRequirements' },
{ 'name': 'vkGetDeviceQueue' },
{ 'name': 'vkGetFenceStatus' },
{ 'name': 'vkGetImageMemoryRequirements' },
{ 'name': 'vkMapMemory' },
{ 'name': 'vkResetFences' },
{ 'name': 'vkUnmapMemory' },
{ 'name': 'vkUpdateDescriptorSets' },
{ 'name': 'vkWaitForFences' },
]

VULKAN_DEVICE_FUNCTIONS_ANDROID = [
{ 'name': 'vkGetAndroidHardwareBufferPropertiesANDROID' },
]

VULKAN_PHYSICAL_DEVICE_FUNCTIONS_ANDROID = [
{ 'name': 'vkGetPhysicalDeviceFeatures2' },
]

VULKAN_DEVICE_FUNCTIONS_LINUX_OR_ANDROID = [
{ 'name': 'vkGetSemaphoreFdKHR' },
{ 'name': 'vkImportSemaphoreFdKHR' },
]

VULKAN_DEVICE_FUNCTIONS_LINUX = [
{ 'name': 'vkGetMemoryFdKHR'},
]

VULKAN_DEVICE_FUNCTIONS_FUCHSIA = [
{ 'name': 'vkImportSemaphoreZirconHandleFUCHSIA' },
{ 'name': 'vkGetSemaphoreZirconHandleFUCHSIA' },
{ 'name': 'vkCreateBufferCollectionFUCHSIA' },
{ 'name': 'vkSetBufferCollectionConstraintsFUCHSIA' },
{ 'name': 'vkGetBufferCollectionPropertiesFUCHSIA' },
{ 'name': 'vkDestroyBufferCollectionFUCHSIA' },
]

VULKAN_QUEUE_FUNCTIONS = [
{ 'name': 'vkQueueSubmit' },
{ 'name': 'vkQueueWaitIdle' },
]

VULKAN_COMMAND_BUFFER_FUNCTIONS = [
{ 'name': 'vkBeginCommandBuffer' },
{ 'name': 'vkCmdBeginRenderPass' },
{ 'name': 'vkCmdCopyBufferToImage' },
{ 'name': 'vkCmdEndRenderPass' },
{ 'name': 'vkCmdExecuteCommands' },
{ 'name': 'vkCmdNextSubpass' },
{ 'name': 'vkCmdPipelineBarrier' },
{ 'name': 'vkEndCommandBuffer' },
{ 'name': 'vkResetCommandBuffer' },
]

VULKAN_SWAPCHAIN_FUNCTIONS = [
{ 'name': 'vkAcquireNextImageKHR' },
{ 'name': 'vkCreateSwapchainKHR' },
{ 'name': 'vkDestroySwapchainKHR' },
{ 'name': 'vkGetSwapchainImagesKHR' },
{ 'name': 'vkQueuePresentKHR' },
]

SELF_LOCATION = os.path.dirname(os.path.abspath(__file__))

LICENSE_AND_HEADER = """\
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is auto-generated from
// gpu/vulkan/generate_bindings.py
// It's formatted by clang-format using chromium coding style:
//    clang-format -i -style=chromium filename
// DO NOT EDIT!

"""

def WriteFunctionDeclarations(file, functions):
  template = Template('  PFN_${name} ${name}Fn = nullptr;\n')
  for func in functions:
    file.write(template.substitute(func))

def WriteMacros(file, functions):
  template = Template(
      '#define $name gpu::GetVulkanFunctionPointers()->${name}Fn\n')
  for func in functions:
    file.write(template.substitute(func))

def GenerateHeaderFile(file, unassociated_functions, instance_functions,
                       physical_device_functions, device_functions,
                       device_functions_android,
                       physical_device_functions_android,
                       device_functions_linux_or_android,
                       device_functions_linux, device_functions_fuchsia,
                       queue_functions, command_buffer_functions,
                       swapchain_functions):
  """Generates gpu/vulkan/vulkan_function_pointers.h"""

  file.write(LICENSE_AND_HEADER +
"""

#ifndef GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
#define GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_

#include <vulkan/vulkan.h>

#include "base/native_library.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_export.h"

#if defined(OS_ANDROID)
#include <vulkan/vulkan_android.h>
#endif

#if defined(OS_FUCHSIA)
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#if defined(USE_VULKAN_XLIB)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#endif

namespace gpu {

struct VulkanFunctionPointers;

VULKAN_EXPORT VulkanFunctionPointers* GetVulkanFunctionPointers();

struct VulkanFunctionPointers {
  VulkanFunctionPointers();
  ~VulkanFunctionPointers();

  VULKAN_EXPORT bool BindUnassociatedFunctionPointers();

  // These functions assume that vkGetInstanceProcAddr has been populated.
  VULKAN_EXPORT bool BindInstanceFunctionPointers(VkInstance vk_instance);
  VULKAN_EXPORT bool BindPhysicalDeviceFunctionPointers(VkInstance vk_instance);

  // These functions assume that vkGetDeviceProcAddr has been populated.
  // |using_swiftshader| allows functions that aren't supported by Swiftshader
  // to be missing.
  // TODO(samans): Remove |using_swiftshader| once all the workarounds can be
  // removed. https://crbug.com/963988
  VULKAN_EXPORT bool BindDeviceFunctionPointers(VkDevice vk_device,
                                                bool using_swiftshader = false);
  bool BindSwapchainFunctionPointers(VkDevice vk_device);

  base::NativeLibrary vulkan_loader_library_ = nullptr;

  // Unassociated functions
  PFN_vkEnumerateInstanceVersion vkEnumerateInstanceVersionFn = nullptr;
  PFN_vkGetInstanceProcAddr vkGetInstanceProcAddrFn = nullptr;
""")

  WriteFunctionDeclarations(file, unassociated_functions)

  file.write("""\

  // Instance functions
""")

  WriteFunctionDeclarations(file, instance_functions)

  file.write("""\
  PFN_vkDestroySurfaceKHR vkDestroySurfaceKHRFn = nullptr;
#if defined(USE_VULKAN_XLIB)
  PFN_vkCreateXlibSurfaceKHR vkCreateXlibSurfaceKHRFn = nullptr;
#endif
  // Physical Device functions
""")

  WriteFunctionDeclarations(file, physical_device_functions)

  file.write("""\
  PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
      vkGetPhysicalDeviceSurfaceCapabilitiesKHRFn = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
      vkGetPhysicalDeviceSurfaceFormatsKHRFn = nullptr;
  PFN_vkGetPhysicalDeviceSurfaceSupportKHR
      vkGetPhysicalDeviceSurfaceSupportKHRFn = nullptr;
#if defined(USE_VULKAN_XLIB)
  PFN_vkGetPhysicalDeviceXlibPresentationSupportKHR
      vkGetPhysicalDeviceXlibPresentationSupportKHRFn = nullptr;
#endif
  // Device functions
""")

  WriteFunctionDeclarations(file, device_functions)

  file.write("""\

#if defined(OS_ANDROID)
  // Android only device functions.
""")

  WriteFunctionDeclarations(file, device_functions_android)

  file.write("""\

  // Android only physical device functions.
""")

  WriteFunctionDeclarations(file, physical_device_functions_android)

  file.write("""\
#endif
""")

  file.write("""\

  // Device functions shared between Linux and Android.
#if defined(OS_LINUX) || defined(OS_ANDROID)
""")

  WriteFunctionDeclarations(file, device_functions_linux_or_android)

  file.write("""\
#endif
""")

  file.write("""\

  // Linux-only device functions.
#if defined(OS_LINUX)
""")

  WriteFunctionDeclarations(file, device_functions_linux)

  file.write("""\
#endif
""")

  file.write("""\

  // Fuchsia only device functions.
#if defined(OS_FUCHSIA)
""")

  WriteFunctionDeclarations(file, device_functions_fuchsia)

  file.write("""\
#endif
""")

  file.write("""\

  // Queue functions
""")

  WriteFunctionDeclarations(file, queue_functions)

  file.write("""\

  // Command Buffer functions
""")

  WriteFunctionDeclarations(file, command_buffer_functions)

  file.write("""\

  // Swapchain functions
""")

  WriteFunctionDeclarations(file, swapchain_functions)

  file.write("""\
};

}  // namespace gpu

// Unassociated functions
""")

  WriteMacros(file, [ { 'name': 'vkGetInstanceProcAddr' } ])
  WriteMacros(file, unassociated_functions)

  file.write("""\

// Instance functions
""")

  WriteMacros(file, instance_functions)
  WriteMacros(file, [ { 'name': 'vkDestroySurfaceKHR' } ])

  file.write("#if defined(USE_VULKAN_XLIB)\n")
  WriteMacros(file, [ { 'name': 'vkCreateXlibSurfaceKHR' } ])
  file.write("#endif\n")

  file.write("""\

// Physical Device functions
""")

  WriteMacros(file, physical_device_functions)
  WriteMacros(file, [
      { 'name': 'vkGetPhysicalDeviceSurfaceCapabilitiesKHR' },
      { 'name': 'vkGetPhysicalDeviceSurfaceFormatsKHR' },
      { 'name': 'vkGetPhysicalDeviceSurfaceSupportKHR' },
  ])
  file.write("#if defined(USE_VULKAN_XLIB)\n")
  WriteMacros(file, [
      { 'name': 'vkGetPhysicalDeviceXlibPresentationSupportKHR' },
  ])
  file.write("#endif\n")

  file.write("""\

#if defined(OS_ANDROID)
""")

  WriteMacros(file, physical_device_functions_android)

  file.write("""\
#endif
""")

  file.write("""\

// Device functions
""")

  WriteMacros(file, device_functions)

  file.write("""\

#if defined(OS_ANDROID)
""")

  WriteMacros(file, device_functions_android)

  file.write("""\
#endif
""")

  file.write("""\

#if defined(OS_LINUX) || defined(OS_ANDROID)
""")

  WriteMacros(file, device_functions_linux_or_android)

  file.write("""\
#endif
""")

  file.write("""\

#if defined(OS_LINUX)
""")

  WriteMacros(file, device_functions_linux)

  file.write("""\
#endif
""")

  file.write("""\

#if defined(OS_FUCHSIA)
""")

  WriteMacros(file, device_functions_fuchsia)

  file.write("""\
#endif
""")

  file.write("""\

// Queue functions
""")

  WriteMacros(file, queue_functions)

  file.write("""\

// Command buffer functions
""")

  WriteMacros(file, command_buffer_functions)

  file.write("""\

// Swapchain functions
""")

  WriteMacros(file, swapchain_functions)

  file.write("""\

#endif  // GPU_VULKAN_VULKAN_FUNCTION_POINTERS_H_
""")

def WriteFunctionPointerInitialization(file, proc_addr_function, parent,
                                       functions, allow_missing=False):
  template = Template("""  ${name}Fn = reinterpret_cast<PFN_${name}>(
    $get_proc_addr($parent, "$name"));
  if (!${name}Fn${check_swiftshader}) {
    DLOG(WARNING) << "Failed to bind vulkan entrypoint: " << "${name}";
    return false;
  }

""")
  if allow_missing:
    check_swiftshader = " && !using_swiftshader"
  else:
    check_swiftshader = ""
  for func in functions:
    file.write(template.substitute(name=func['name'], get_proc_addr =
                                   proc_addr_function, parent=parent,
                                   check_swiftshader=check_swiftshader))

def WriteUnassociatedFunctionPointerInitialization(file, functions):
  WriteFunctionPointerInitialization(file, 'vkGetInstanceProcAddrFn', 'nullptr',
                                     functions)

def WriteInstanceFunctionPointerInitialization(file, functions):
  WriteFunctionPointerInitialization(file, 'vkGetInstanceProcAddrFn',
                                     'vk_instance', functions)

def WriteDeviceFunctionPointerInitialization(file,
                                             functions,
                                             allow_missing=False):
  WriteFunctionPointerInitialization(file, 'vkGetDeviceProcAddrFn', 'vk_device',
                                     functions, allow_missing)

def GenerateSourceFile(file, unassociated_functions, instance_functions,
                       physical_device_functions, device_functions,
                       device_functions_android,
                       physical_device_functions_android,
                       device_functions_linux_or_android,
                       device_functions_linux, device_functions_fuchsia,
                       queue_functions, command_buffer_functions,
                       swapchain_functions):
  """Generates gpu/vulkan/vulkan_function_pointers.cc"""

  file.write(LICENSE_AND_HEADER +
"""

#include "gpu/vulkan/vulkan_function_pointers.h"

#include "base/no_destructor.h"

namespace gpu {

VulkanFunctionPointers* GetVulkanFunctionPointers() {
  static base::NoDestructor<VulkanFunctionPointers> vulkan_function_pointers;
  return vulkan_function_pointers.get();
}

VulkanFunctionPointers::VulkanFunctionPointers() = default;
VulkanFunctionPointers::~VulkanFunctionPointers() = default;

bool VulkanFunctionPointers::BindUnassociatedFunctionPointers() {
  // vkGetInstanceProcAddr must be handled specially since it gets its function
  // pointer through base::GetFunctionPOinterFromNativeLibrary(). Other Vulkan
  // functions don't do this.
  vkGetInstanceProcAddrFn = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
      base::GetFunctionPointerFromNativeLibrary(vulkan_loader_library_,
                                                "vkGetInstanceProcAddr"));
  if (!vkGetInstanceProcAddrFn)
    return false;

  vkEnumerateInstanceVersionFn =
      reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
          vkGetInstanceProcAddrFn(nullptr, "vkEnumerateInstanceVersion"));
  // vkEnumerateInstanceVersion didn't exist in Vulkan 1.0, so we should
  // proceed even if we fail to get vkEnumerateInstanceVersion pointer.
""")

  WriteUnassociatedFunctionPointerInitialization(file, unassociated_functions)

  file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindInstanceFunctionPointers(
    VkInstance vk_instance) {
""")

  WriteInstanceFunctionPointerInitialization(file, instance_functions)

  file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindPhysicalDeviceFunctionPointers(
    VkInstance vk_instance) {
""")

  WriteInstanceFunctionPointerInitialization(file, physical_device_functions)

  file.write("""\
#if defined(OS_ANDROID)
""")

  WriteInstanceFunctionPointerInitialization(file,
                                             physical_device_functions_android)

  file.write("""\
#endif
""")

  file.write("""\

  return true;
}

bool VulkanFunctionPointers::BindDeviceFunctionPointers(
    VkDevice vk_device,
    bool using_swiftshader) {
  // Device functions
""")
  WriteDeviceFunctionPointerInitialization(file, device_functions)

  file.write("""\

#if defined(OS_ANDROID)

""")

  WriteDeviceFunctionPointerInitialization(file, device_functions_android)

  file.write("""\
#endif
""")

  file.write("""\

#if defined(OS_LINUX) || defined(OS_ANDROID)

""")

  WriteDeviceFunctionPointerInitialization(file,
                                           device_functions_linux_or_android,
                                           True) # allow_missing

  file.write("""\
#endif
""")

  file.write("""\

#if defined(OS_LINUX)

""")

  WriteDeviceFunctionPointerInitialization(file,
                                           device_functions_linux,
                                           True) # allow_missing

  file.write("""\
#endif
""")

  file.write("""\

#if defined(OS_FUCHSIA)

""")

  WriteDeviceFunctionPointerInitialization(file, device_functions_fuchsia)

  file.write("""\
#endif
""")

  file.write("""\

  // Queue functions
""")
  WriteDeviceFunctionPointerInitialization(file, queue_functions)

  file.write("""\

  // Command Buffer functions
""")
  WriteDeviceFunctionPointerInitialization(file, command_buffer_functions)

  file.write("""\


  return true;
}

bool VulkanFunctionPointers::BindSwapchainFunctionPointers(VkDevice vk_device) {
""")

  WriteDeviceFunctionPointerInitialization(file, swapchain_functions)

  file.write("""\


  return true;
}

}  // namespace gpu
""")

def main(argv):
  """This is the main function."""

  parser = optparse.OptionParser()
  parser.add_option(
      "--output-dir",
      help="Output directory for generated files. Defaults to this script's "
      "directory.")
  parser.add_option(
      "-c", "--check", action="store_true",
      help="Check if output files match generated files in chromium root "
      "directory. Use this in PRESUBMIT scripts with --output-dir.")

  (options, _) = parser.parse_args(args=argv)

  # Support generating files for PRESUBMIT.
  if options.output_dir:
    output_dir = options.output_dir
  else:
    output_dir = SELF_LOCATION

  def ClangFormat(filename):
    formatter = "clang-format"
    if platform.system() == "Windows":
      formatter += ".bat"
    call([formatter, "-i", "-style=chromium", filename])

  header_file_name = 'vulkan_function_pointers.h'
  header_file = open(
      os.path.join(output_dir, header_file_name), 'wb')
  GenerateHeaderFile(header_file, VULKAN_UNASSOCIATED_FUNCTIONS,
                     VULKAN_INSTANCE_FUNCTIONS,
                     VULKAN_PHYSICAL_DEVICE_FUNCTIONS, VULKAN_DEVICE_FUNCTIONS,
                     VULKAN_DEVICE_FUNCTIONS_ANDROID,
                     VULKAN_PHYSICAL_DEVICE_FUNCTIONS_ANDROID,
                     VULKAN_DEVICE_FUNCTIONS_LINUX_OR_ANDROID,
                     VULKAN_DEVICE_FUNCTIONS_LINUX,
                     VULKAN_DEVICE_FUNCTIONS_FUCHSIA,
                     VULKAN_QUEUE_FUNCTIONS, VULKAN_COMMAND_BUFFER_FUNCTIONS,
                     VULKAN_SWAPCHAIN_FUNCTIONS)
  header_file.close()
  ClangFormat(header_file.name)

  source_file_name = 'vulkan_function_pointers.cc'
  source_file = open(
      os.path.join(output_dir, source_file_name), 'wb')
  GenerateSourceFile(source_file, VULKAN_UNASSOCIATED_FUNCTIONS,
                     VULKAN_INSTANCE_FUNCTIONS,
                     VULKAN_PHYSICAL_DEVICE_FUNCTIONS, VULKAN_DEVICE_FUNCTIONS,
                     VULKAN_DEVICE_FUNCTIONS_ANDROID,
                     VULKAN_PHYSICAL_DEVICE_FUNCTIONS_ANDROID,
                     VULKAN_DEVICE_FUNCTIONS_LINUX_OR_ANDROID,
                     VULKAN_DEVICE_FUNCTIONS_LINUX,
                     VULKAN_DEVICE_FUNCTIONS_FUCHSIA,
                     VULKAN_QUEUE_FUNCTIONS, VULKAN_COMMAND_BUFFER_FUNCTIONS,
                     VULKAN_SWAPCHAIN_FUNCTIONS)
  source_file.close()
  ClangFormat(source_file.name)

  check_failed_filenames = []
  if options.check:
    for filename in [header_file_name, source_file_name]:
      if not filecmp.cmp(os.path.join(output_dir, filename),
                         os.path.join(SELF_LOCATION, filename)):
        check_failed_filenames.append(filename)

  if len(check_failed_filenames) > 0:
    print 'Please run gpu/vulkan/generate_bindings.py'
    print 'Failed check on generated files:'
    for filename in check_failed_filenames:
      print filename
    return 1

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
