// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/vulkan_swap_chain.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"

#if defined(USE_X11)
#include "ui/base/ui_base_features.h"  // nogncheck
#endif

namespace gpu {

namespace {

VkSemaphore CreateSemaphore(VkDevice vk_device) {
  // Generic semaphore creation structure.
  VkSemaphoreCreateInfo semaphore_create_info = {
      VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

  VkSemaphore vk_semaphore;
  auto result = vkCreateSemaphore(vk_device, &semaphore_create_info, nullptr,
                                  &vk_semaphore);
  LOG_IF(FATAL, VK_SUCCESS != result)
      << "vkCreateSemaphore() failed: " << result;
  return vk_semaphore;
}

}  // namespace

VulkanSwapChain::VulkanSwapChain() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

VulkanSwapChain::~VulkanSwapChain() {
#if DCHECK_IS_ON()
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(images_.empty());
  DCHECK_EQ(static_cast<VkSwapchainKHR>(VK_NULL_HANDLE), swap_chain_);
#endif
}

bool VulkanSwapChain::Initialize(
    VulkanDeviceQueue* device_queue,
    VkSurfaceKHR surface,
    const VkSurfaceFormatKHR& surface_format,
    const gfx::Size& image_size,
    uint32_t min_image_count,
    VkSurfaceTransformFlagBitsKHR pre_transform,
    bool use_protected_memory,
    std::unique_ptr<VulkanSwapChain> old_swap_chain) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(device_queue);
  DCHECK(!use_protected_memory || device_queue->allow_protected_memory());

  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  use_protected_memory_ = use_protected_memory;
  device_queue_ = device_queue;
  is_incremental_present_supported_ =
      gfx::HasExtension(device_queue_->enabled_extensions(),
                        VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);
  device_queue_->GetFenceHelper()->ProcessCleanupTasks();
  return InitializeSwapChain(surface, surface_format, image_size,
                             min_image_count, pre_transform,
                             use_protected_memory, std::move(old_swap_chain)) &&
         InitializeSwapImages(surface_format) && AcquireNextImage();
}

void VulkanSwapChain::Destroy() {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  WaitUntilPostSubBufferAsyncFinished();

  DCHECK(!is_writing_);
  DestroySwapImages();
  DestroySwapChain();
}

gfx::SwapResult VulkanSwapChain::PostSubBuffer(const gfx::Rect& rect) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!PresentBuffer(rect))
    return gfx::SwapResult::SWAP_FAILED;

  if (!AcquireNextImage())
    return gfx::SwapResult::SWAP_FAILED;

  return gfx::SwapResult::SWAP_ACK;
}

void VulkanSwapChain::PostSubBufferAsync(
    const gfx::Rect& rect,
    PostSubBufferCompletionCallback callback) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!PresentBuffer(rect)) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), gfx::SwapResult::SWAP_FAILED));
    return;
  }

  DCHECK_EQ(state_, VK_SUCCESS);

  ++pending_post_sub_buffer_;

  post_sub_buffer_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](VulkanSwapChain* self, PostSubBufferCompletionCallback callback) {
            base::AutoLock auto_lock(self->lock_);
            auto swap_result = self->AcquireNextImage()
                                   ? gfx::SwapResult::SWAP_ACK
                                   : gfx::SwapResult::SWAP_FAILED;
            self->task_runner_->PostTask(
                FROM_HERE, base::BindOnce(std::move(callback), swap_result));
            --self->pending_post_sub_buffer_;
            self->condition_variable_.Signal();
          },
          base::Unretained(this), std::move(callback)));
}

bool VulkanSwapChain::InitializeSwapChain(
    VkSurfaceKHR surface,
    const VkSurfaceFormatKHR& surface_format,
    const gfx::Size& image_size,
    uint32_t min_image_count,
    VkSurfaceTransformFlagBitsKHR pre_transform,
    bool use_protected_memory,
    std::unique_ptr<VulkanSwapChain> old_swap_chain) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VkDevice device = device_queue_->GetVulkanDevice();
  VkResult result = VK_SUCCESS;

  VkSwapchainCreateInfoKHR swap_chain_create_info = {};
  swap_chain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swap_chain_create_info.flags =
      use_protected_memory ? VK_SWAPCHAIN_CREATE_PROTECTED_BIT_KHR : 0;
  swap_chain_create_info.surface = surface;
  swap_chain_create_info.minImageCount = min_image_count,
  swap_chain_create_info.imageFormat = surface_format.format;
  swap_chain_create_info.imageColorSpace = surface_format.colorSpace;
  swap_chain_create_info.imageExtent.width = image_size.width();
  swap_chain_create_info.imageExtent.height = image_size.height();
  swap_chain_create_info.imageArrayLayers = 1;
  swap_chain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swap_chain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swap_chain_create_info.preTransform = pre_transform;
  swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swap_chain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
  swap_chain_create_info.clipped = true;
  if (old_swap_chain) {
    base::AutoLock auto_lock(old_swap_chain->lock_);
    old_swap_chain->WaitUntilPostSubBufferAsyncFinished();
    swap_chain_create_info.oldSwapchain =
        old_swap_chain ? old_swap_chain->swap_chain_ : VK_NULL_HANDLE;
    // Reuse |post_sub_buffer_task_runner_| from the |old_swap_chain|.
    post_sub_buffer_task_runner_ = old_swap_chain->post_sub_buffer_task_runner_;
  }

  VkSwapchainKHR new_swap_chain = VK_NULL_HANDLE;
  result = vkCreateSwapchainKHR(device, &swap_chain_create_info, nullptr,
                                &new_swap_chain);

  if (old_swap_chain) {
    auto* fence_helper = device_queue_->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(old_swap_chain));
  }

  if (VK_SUCCESS != result) {
    LOG(FATAL) << "vkCreateSwapchainKHR() failed: " << result;
    return false;
  }

  swap_chain_ = new_swap_chain;
  size_ = gfx::Size(swap_chain_create_info.imageExtent.width,
                    swap_chain_create_info.imageExtent.height);

  if (!post_sub_buffer_task_runner_) {
    post_sub_buffer_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
        {base::TaskPriority::USER_BLOCKING,
         base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()});
  }

  return true;
}

void VulkanSwapChain::DestroySwapChain() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (swap_chain_ == VK_NULL_HANDLE)
    return;
  vkDestroySwapchainKHR(device_queue_->GetVulkanDevice(), swap_chain_,
                        nullptr /* pAllocator */);
  swap_chain_ = VK_NULL_HANDLE;
}

bool VulkanSwapChain::InitializeSwapImages(
    const VkSurfaceFormatKHR& surface_format) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  VkDevice device = device_queue_->GetVulkanDevice();
  VkResult result = VK_SUCCESS;

  uint32_t image_count = 0;
  result = vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, nullptr);
  if (VK_SUCCESS != result) {
    LOG(FATAL) << "vkGetSwapchainImagesKHR(nullptr) failed: " << result;
    return false;
  }

  std::vector<VkImage> images(image_count);
  result =
      vkGetSwapchainImagesKHR(device, swap_chain_, &image_count, images.data());
  if (VK_SUCCESS != result) {
    LOG(FATAL) << "vkGetSwapchainImagesKHR(images) failed: " << result;
    return false;
  }

  command_pool_ = device_queue_->CreateCommandPool();
  if (!command_pool_)
    return false;

  images_.resize(image_count);
  for (uint32_t i = 0; i < image_count; ++i) {
    auto& image_data = images_[i];
    image_data.image = images[i];
    // Initialize the command buffer for this buffer data.
    image_data.command_buffer = command_pool_->CreatePrimaryCommandBuffer();
  }
  return true;
}

void VulkanSwapChain::DestroySwapImages() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (end_write_semaphore_)
    vkDestroySemaphore(device_queue_->GetVulkanDevice(), end_write_semaphore_,
                       nullptr /* pAllocator */);
  end_write_semaphore_ = VK_NULL_HANDLE;

  for (auto& image_data : images_) {
    if (image_data.command_buffer) {
      image_data.command_buffer->Destroy();
      image_data.command_buffer = nullptr;
    }
    if (image_data.present_begin_semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device_queue_->GetVulkanDevice(),
                         image_data.present_begin_semaphore,
                         nullptr /* pAllocator */);
    }
    if (image_data.present_end_semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device_queue_->GetVulkanDevice(),
                         image_data.present_end_semaphore,
                         nullptr /* pAllocator */);
    }
  }
  images_.clear();

  command_pool_->Destroy();
  command_pool_ = nullptr;
}

bool VulkanSwapChain::BeginWriteCurrentImage(VkImage* image,
                                             uint32_t* image_index,
                                             VkImageLayout* image_layout,
                                             VkSemaphore* semaphore) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(image);
  DCHECK(image_index);
  DCHECK(image_layout);
  DCHECK(semaphore);
  DCHECK(!is_writing_);

  while (state_ == VK_SUCCESS && acquired_images_.empty()) {
    DCHECK(pending_post_sub_buffer_);
    condition_variable_.Wait();
  }

  if (state_ != VK_SUCCESS)
    return false;

  DCHECK(!acquired_images_.empty());
  auto& current_image_data = images_[acquired_images_.front()];

  VkSemaphore vk_semaphore = VK_NULL_HANDLE;
  if (current_image_data.present_end_semaphore != VK_NULL_HANDLE) {
    DCHECK(end_write_semaphore_ == VK_NULL_HANDLE);
    vk_semaphore = current_image_data.present_end_semaphore;
    current_image_data.present_end_semaphore = VK_NULL_HANDLE;
  } else {
    DCHECK(end_write_semaphore_ != VK_NULL_HANDLE);
    // In this case, PostSubBuffer() is not called after
    // {Begin,End}WriteCurrentImage pairs, |end_write_semaphore_| should be
    // waited on before writing the image again.
    vk_semaphore = end_write_semaphore_;
    end_write_semaphore_ = VK_NULL_HANDLE;
  }

  *image = current_image_data.image;
  *image_index = acquired_images_.front();
  *image_layout = current_image_data.layout;
  *semaphore = vk_semaphore;
  is_writing_ = true;

  return true;
}

void VulkanSwapChain::EndWriteCurrentImage(VkImageLayout image_layout,
                                           VkSemaphore semaphore) {
  base::AutoLock auto_lock(lock_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(is_writing_);
  DCHECK(!acquired_images_.empty());
  DCHECK(end_write_semaphore_ == VK_NULL_HANDLE);

  auto& current_image_data = images_[acquired_images_.front()];
  current_image_data.layout = image_layout;
  end_write_semaphore_ = semaphore;
  is_writing_ = false;
}

bool VulkanSwapChain::PresentBuffer(const gfx::Rect& rect) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(state_, VK_SUCCESS);
  DCHECK(!acquired_images_.empty());
  DCHECK(end_write_semaphore_ != VK_NULL_HANDLE);

  VkResult result = VK_SUCCESS;
  VkDevice device = device_queue_->GetVulkanDevice();
  VkQueue queue = device_queue_->GetVulkanQueue();
  auto* fence_helper = device_queue_->GetFenceHelper();

  auto& current_image_data = images_[acquired_images_.front()];
  if (current_image_data.layout != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
    {
      current_image_data.command_buffer->Clear();
      ScopedSingleUseCommandBufferRecorder recorder(
          *current_image_data.command_buffer);
      current_image_data.command_buffer->TransitionImageLayout(
          current_image_data.image, current_image_data.layout,
          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }
    current_image_data.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkSemaphore vk_semaphore = CreateSemaphore(device);
    // Submit our command_buffer for the current buffer. It sets the image
    // layout for presenting.
    if (!current_image_data.command_buffer->Submit(1, &end_write_semaphore_, 1,
                                                   &vk_semaphore)) {
      vkDestroySemaphore(device, vk_semaphore, nullptr /* pAllocator */);
      return false;
    }
    current_image_data.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    fence_helper->EnqueueSemaphoreCleanupForSubmittedWork(end_write_semaphore_);
    end_write_semaphore_ = vk_semaphore;
  }

  VkPresentInfoKHR present_info = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &end_write_semaphore_;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swap_chain_;
  present_info.pImageIndices = &acquired_images_.front();

  VkRectLayerKHR rect_layer;
  VkPresentRegionKHR present_region;
  VkPresentRegionsKHR present_regions = {VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR};
  if (is_incremental_present_supported_) {
    rect_layer.offset = {rect.x(), rect.y()};
    rect_layer.extent = {rect.width(), rect.height()};
    rect_layer.layer = 0;

    present_region.rectangleCount = 1;
    present_region.pRectangles = &rect_layer;

    present_regions.swapchainCount = 1;
    present_regions.pRegions = &present_region;

    present_info.pNext = &present_regions;
  }

  result = vkQueuePresentKHR(queue, &present_info);
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    LOG(DFATAL) << "vkQueuePresentKHR() failed: " << result;
    state_ = result;
    return false;
  }

  current_image_data.is_acquired = false;
  LOG_IF(ERROR, result == VK_SUBOPTIMAL_KHR) << "Swapchian is suboptimal.";

  if (current_image_data.present_begin_semaphore != VK_NULL_HANDLE) {
    // |present_begin_semaphore| for the previous present for this image can be
    // safely destroyed after semaphore got from vkAcquireNextImageHKR() is
    // passed. That acquired semaphore should be already waited on for a
    // submitted GPU work. So we can safely enqueue the
    // |present_begin_semaphore| for cleanup here (the enqueued semaphore will
    // be destroyed when all submitted GPU work is finished).
    fence_helper->EnqueueSemaphoreCleanupForSubmittedWork(
        current_image_data.present_begin_semaphore);
  }
  // We are not sure when the semaphore is not used by present engine, so don't
  // destroy the semaphore until the image is returned from present engine.
  current_image_data.present_begin_semaphore = end_write_semaphore_;
  end_write_semaphore_ = VK_NULL_HANDLE;

  in_present_images_.emplace_back(acquired_images_.front());
  acquired_images_.pop_front();

  return true;
}

bool VulkanSwapChain::AcquireNextImage() {
  DCHECK_EQ(state_, VK_SUCCESS);
  DCHECK_LT(acquired_images_.size(), images_.size());

  // VulkanDeviceQueue is not threadsafe for now, but |device_queue_| will not
  // be released, and device_queue_->device will never be changed after
  // initialization, so it is safe for now.
  // TODO(penghuang): make VulkanDeviceQueue threadsafe.
  VkDevice device = device_queue_->GetVulkanDevice();

  // The Vulkan spec doesn't require vkAcquireNextImageKHR() returns images in
  // the present order for a vulkan swap chain. However for the best
  // performance, the driver should return images in order. To avoid buggy
  // drivers, we will call vkAcquireNextImageKHR() continually until the
  // expected image is returned.
  do {
    bool all_images_are_tracked = in_present_images_.size() == images_.size();
    if (all_images_are_tracked) {
      // Only check the expected_next_image, when all images are tracked.
      uint32_t expected_next_image = in_present_images_.front();
      // If the expected next image has been acquired, use it and return true.
      if (images_[expected_next_image].is_acquired) {
        in_present_images_.pop_front();
        acquired_images_.emplace_back(expected_next_image);
        break;
      }
    }

    VkSemaphore vk_semaphore = CreateSemaphore(device);
    DCHECK(vk_semaphore != VK_NULL_HANDLE);

    uint64_t kTimeout = UINT64_MAX;
#if defined(USE_X11)
    // The xserver should still composite windows with a 1Hz fake vblank when
    // screen is off or the window is offscreen. However there is an xserver
    // bug, the requested hardware vblanks are lost, when screen turns off, so
    // FIFO swapchain will hang.
    // Workaround the issue by using the 2 seconds timeout for
    // vkAcquireNextImageKHR(). When timeout happens, we consider the swapchain
    // hang happened, and then make the surface lost, so a new swapchain will
    // be recreated.
    //
    // TODO(https://crbug.com/1098237): set correct timeout for ozone/x11.
    if (!features::IsUsingOzonePlatform())
      kTimeout = base::Time::kNanosecondsPerSecond * 2;
#endif
    // Acquire the next image.
    uint32_t next_image;
    auto result = ({
      base::ScopedBlockingCall scoped_blocking_call(
          FROM_HERE, base::BlockingType::WILL_BLOCK);
      vkAcquireNextImageKHR(device, swap_chain_, kTimeout, vk_semaphore,
                            VK_NULL_HANDLE, &next_image);
    });

    if (result == VK_TIMEOUT) {
      LOG(ERROR) << "vkAcquireNextImageKHR() hangs.";
      vkDestroySemaphore(device, vk_semaphore, nullptr /* pAllocator */);
      state_ = VK_ERROR_SURFACE_LOST_KHR;
      return false;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
      LOG(DFATAL) << "vkAcquireNextImageKHR() failed: " << result;
      vkDestroySemaphore(device, vk_semaphore, nullptr /* pAllocator */);
      state_ = result;
      return false;
    }

    DCHECK(!images_[next_image].is_acquired);
    DCHECK(images_[next_image].present_end_semaphore == VK_NULL_HANDLE);
    images_[next_image].is_acquired = true;
    images_[next_image].present_end_semaphore = vk_semaphore;

    auto it = std::find(in_present_images_.begin(), in_present_images_.end(),
                        next_image);
    if (it == in_present_images_.end()) {
      DCHECK(!all_images_are_tracked);
      // Got an image which is not in the present queue due to the new created
      // swap chain. In this case, just use this image.
      acquired_images_.emplace_back(next_image);
      break;
    }
    LOG_IF(ERROR, it != in_present_images_.begin())
        << "vkAcquireNextImageKHR() returned an unexpected image.";
  } while (true);
  return true;
}

void VulkanSwapChain::WaitUntilPostSubBufferAsyncFinished() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  while (pending_post_sub_buffer_) {
    base::ScopedBlockingCall scoped_blocking_call(
        FROM_HERE, base::BlockingType::WILL_BLOCK);
    condition_variable_.Wait();
  }
  DCHECK(!acquired_images_.empty() || state_ != VK_SUCCESS);
}

VulkanSwapChain::ScopedWrite::ScopedWrite(VulkanSwapChain* swap_chain)
    : swap_chain_(swap_chain) {
  success_ = swap_chain_->BeginWriteCurrentImage(
      &image_, &image_index_, &image_layout_, &begin_semaphore_);
}

VulkanSwapChain::ScopedWrite::~ScopedWrite() {
  DCHECK(begin_semaphore_ == VK_NULL_HANDLE);
  if (success_)
    swap_chain_->EndWriteCurrentImage(image_layout_, end_semaphore_);
}

VkSemaphore VulkanSwapChain::ScopedWrite::TakeBeginSemaphore() {
  VkSemaphore semaphore = begin_semaphore_;
  begin_semaphore_ = VK_NULL_HANDLE;
  return semaphore;
}

VkSemaphore VulkanSwapChain::ScopedWrite::GetEndSemaphore() {
  DCHECK(end_semaphore_ == VK_NULL_HANDLE);
  end_semaphore_ =
      CreateSemaphore(swap_chain_->device_queue_->GetVulkanDevice());
  return end_semaphore_;
}

VulkanSwapChain::ImageData::ImageData() = default;
VulkanSwapChain::ImageData::ImageData(ImageData&& other) = default;
VulkanSwapChain::ImageData::~ImageData() = default;
VulkanSwapChain::ImageData& VulkanSwapChain::ImageData::operator=(
    ImageData&& other) = default;

}  // namespace gpu
