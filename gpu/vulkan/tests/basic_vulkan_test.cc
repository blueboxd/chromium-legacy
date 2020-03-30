// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/basic_vulkan_test.h"

#include "base/command_line.h"
#include "gpu/vulkan/init/vulkan_factory.h"
#include "gpu/vulkan/tests/native_window.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/rect.h"

#if defined(OS_ANDROID)
#include <android/native_window_jni.h>
#endif

namespace gpu {

BasicVulkanTest::BasicVulkanTest() {}

BasicVulkanTest::~BasicVulkanTest() {}

void BasicVulkanTest::SetUp() {
  platform_event_source_ = ui::PlatformEventSource::CreateDefault();
#if defined(USE_X11) || defined(OS_WIN)
  bool use_swiftshader =
      base::CommandLine::ForCurrentProcess()->HasSwitch("use-swiftshader");
  const gfx::Rect kDefaultBounds(10, 10, 100, 100);
  window_ = CreateNativeWindow(kDefaultBounds);
#elif defined(OS_ANDROID)
  // Vulkan swiftshader is not supported on Android.
  bool use_swiftshader = false;
  // TODO(penghuang): Not depend on gl.
  uint texture = 0;
  surface_texture_ = gl::SurfaceTexture::Create(texture);
  window_ = surface_texture_->CreateSurface();
  ASSERT_TRUE(window_ != gfx::kNullAcceleratedWidget);
#endif
  vulkan_implementation_ = CreateVulkanImplementation(use_swiftshader);
  ASSERT_TRUE(vulkan_implementation_);
  ASSERT_TRUE(vulkan_implementation_->InitializeVulkanInstance());
  device_queue_ = gpu::CreateVulkanDeviceQueue(
      vulkan_implementation_.get(),
      VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
          VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG);
  ASSERT_TRUE(device_queue_);
}

void BasicVulkanTest::TearDown() {
#if defined(USE_X11) || defined(OS_WIN)
  DestroyNativeWindow(window_);
#elif defined(OS_ANDROID)
  ANativeWindow_release(window_);
#endif
  window_ = gfx::kNullAcceleratedWidget;
  device_queue_->Destroy();
  vulkan_implementation_.reset();
  platform_event_source_.reset();
}

std::unique_ptr<VulkanSurface> BasicVulkanTest::CreateViewSurface(
    gfx::AcceleratedWidget window) {
  return vulkan_implementation_->CreateViewSurface(window);
}

}  // namespace gpu
