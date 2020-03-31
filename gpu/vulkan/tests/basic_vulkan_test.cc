// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/basic_vulkan_test.h"

#include "base/command_line.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/tests/native_window.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "ui/gfx/geometry/rect.h"

namespace gpu {

BasicVulkanTest::BasicVulkanTest() {}

BasicVulkanTest::~BasicVulkanTest() {}

void BasicVulkanTest::SetUp() {
#if defined(USE_X11) || defined(OS_WIN) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  bool use_swiftshader =
      base::CommandLine::ForCurrentProcess()->HasSwitch("use-swiftshader");
  const gfx::Rect kDefaultBounds(10, 10, 100, 100);
  window_ = CreateNativeWindow(kDefaultBounds);
  ASSERT_TRUE(window_ != gfx::kNullAcceleratedWidget);
  vulkan_implementation_ = CreateVulkanImplementation(use_swiftshader);
  ASSERT_TRUE(vulkan_implementation_);
  ASSERT_TRUE(vulkan_implementation_->InitializeVulkanInstance());
  device_queue_ = gpu::CreateVulkanDeviceQueue(
      vulkan_implementation_.get(),
      VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
          VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG);
  ASSERT_TRUE(device_queue_);
#else
#error Unsupported platform
#endif
}

void BasicVulkanTest::TearDown() {
#if defined(USE_X11) || defined(OS_WIN) || defined(OS_ANDROID) || \
    defined(USE_OZONE)
  DestroyNativeWindow(window_);
  window_ = gfx::kNullAcceleratedWidget;
  device_queue_->Destroy();
  vulkan_implementation_.reset();
#else
#error Unsupported platform
#endif
}

std::unique_ptr<VulkanSurface> BasicVulkanTest::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  return vulkan_implementation_->CreateViewSurface(window);
}

}  // namespace gpu
