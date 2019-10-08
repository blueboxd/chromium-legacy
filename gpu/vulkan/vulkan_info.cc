// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_info.h"

namespace gpu {

VulkanInfo::VulkanInfo() = default;
VulkanInfo::~VulkanInfo() = default;
VulkanInfo::PhysicalDeviceInfo::PhysicalDeviceInfo() = default;
VulkanInfo::PhysicalDeviceInfo::PhysicalDeviceInfo(
    const PhysicalDeviceInfo& other) = default;
VulkanInfo::PhysicalDeviceInfo::~PhysicalDeviceInfo() = default;
VulkanInfo::PhysicalDeviceInfo& VulkanInfo::PhysicalDeviceInfo::operator=(
    const PhysicalDeviceInfo& info) = default;

}  // namespace gpu
