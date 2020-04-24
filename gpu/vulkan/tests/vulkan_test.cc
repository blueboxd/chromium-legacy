// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/optional.h"
#include "gpu/vulkan/tests/basic_vulkan_test.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_surface.h"
#include "gpu/vulkan/vulkan_swap_chain.h"
#include "gpu/vulkan/vulkan_util.h"

// This file tests basic vulkan initialization steps.

namespace gpu {

TEST_F(BasicVulkanTest, BasicVulkanSurface) {
  if (!supports_swapchain())
    return;
  std::unique_ptr<VulkanSurface> surface = CreateViewSurface(window());
  EXPECT_TRUE(surface);
  EXPECT_TRUE(surface->Initialize(GetDeviceQueue(),
                                  VulkanSurface::DEFAULT_SURFACE_FORMAT));
  EXPECT_TRUE(
      surface->Reshape(gfx::Size(100, 100), gfx::OVERLAY_TRANSFORM_NONE));
  surface->Destroy();
}

TEST_F(BasicVulkanTest, EmptyVulkanSwaps) {
  if (!supports_swapchain())
    return;

  std::unique_ptr<VulkanSurface> surface = CreateViewSurface(window());
  ASSERT_TRUE(surface);
  ASSERT_TRUE(surface->Initialize(GetDeviceQueue(),
                                  VulkanSurface::DEFAULT_SURFACE_FORMAT));
  ASSERT_TRUE(
      surface->Reshape(gfx::Size(100, 100), gfx::OVERLAY_TRANSFORM_NONE));

  constexpr VkSemaphore kNullSemaphore = VK_NULL_HANDLE;
  auto* fence_helper = GetDeviceQueue()->GetFenceHelper();

  base::Optional<VulkanSwapChain::ScopedWrite> scoped_write;
  scoped_write.emplace(surface->swap_chain());
  EXPECT_TRUE(scoped_write->success());
  VkSemaphore begin_semaphore = scoped_write->TakeBeginSemaphore();
  EXPECT_NE(begin_semaphore, kNullSemaphore);
  EXPECT_TRUE(SubmitWaitVkSemaphore(queue(), begin_semaphore));

  fence_helper->EnqueueSemaphoreCleanupForSubmittedWork(begin_semaphore);

  VkSemaphore end_semaphore = scoped_write->GetEndSemaphore();
  EXPECT_NE(end_semaphore, kNullSemaphore);
  EXPECT_TRUE(SubmitSignalVkSemaphore(queue(), end_semaphore));

  scoped_write.reset();

  // First swap is a special case, call it first to get better errors.
  EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers());

  // Also make sure we can swap multiple times.
  for (int i = 0; i < 10; ++i) {
    base::Optional<VulkanSwapChain::ScopedWrite> scoped_write;
    scoped_write.emplace(surface->swap_chain());
    EXPECT_TRUE(scoped_write->success());
    VkSemaphore begin_semaphore = scoped_write->TakeBeginSemaphore();
    EXPECT_NE(begin_semaphore, kNullSemaphore);
    EXPECT_TRUE(SubmitWaitVkSemaphore(queue(), begin_semaphore));

    VkSemaphore end_semaphore = scoped_write->GetEndSemaphore();
    EXPECT_NE(end_semaphore, kNullSemaphore);
    EXPECT_TRUE(SubmitSignalVkSemaphore(queue(), end_semaphore));

    scoped_write.reset();

    EXPECT_EQ(gfx::SwapResult::SWAP_ACK, surface->SwapBuffers());
  }
  surface->Finish();
  surface->Destroy();
}

}  // namespace gpu
