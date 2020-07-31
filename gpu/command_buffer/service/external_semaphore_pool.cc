// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_semaphore_pool.h"

#include "build/build_config.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_implementation.h"

namespace gpu {
namespace {

#if defined(OS_ANDROID)
// On Android, semaphores are created with handle type
// VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT. With this handle type,
// the semaphore will not be reset to un-signalled state after waiting,
// so semaphores cannot be reused on Android.
constexpr size_t kMaxSemaphoresInPool = 0;
#else
constexpr size_t kMaxSemaphoresInPool = 16;
#endif

}  // namespace

ExternalSemaphorePool::ExternalSemaphorePool(
    viz::VulkanContextProvider* context_provider)
    : context_provider_(context_provider) {}

ExternalSemaphorePool::~ExternalSemaphorePool() = default;

ExternalSemaphore ExternalSemaphorePool::GetOrCreateSemaphore() {
  if (!semaphores_.empty()) {
    auto semaphore = std::move(semaphores_.front());
    semaphores_.pop_front();
    return semaphore;
  }
  return ExternalSemaphore::Create(context_provider_);
}

void ExternalSemaphorePool::ReturnSemaphore(ExternalSemaphore semaphore) {
  DCHECK(semaphore);
  if (semaphores_.size() < kMaxSemaphoresInPool)
    semaphores_.push_back(std::move(semaphore));
}

void ExternalSemaphorePool::ReturnSemaphores(
    std::vector<ExternalSemaphore> semaphores) {
  DCHECK_LE(semaphores_.size(), kMaxSemaphoresInPool);

#if DCHECK_IS_ON()
  for (auto& semaphore : semaphores)
    DCHECK(semaphore);
#endif

  std::move(
      semaphores.begin(),
      semaphores.begin() + std::min(kMaxSemaphoresInPool - semaphores_.size(),
                                    semaphores.size()),
      std::back_inserter(semaphores_));
}

void ExternalSemaphorePool::ReturnSemaphoresWithFenceHelper(
    std::vector<ExternalSemaphore> semaphores) {
#if DCHECK_IS_ON()
  for (auto& semaphore : semaphores)
    DCHECK(semaphore);
#endif

  if (semaphores.empty())
    return;
  auto* fence_helper = context_provider_->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueCleanupTaskForSubmittedWork(base::BindOnce(
      [](base::WeakPtr<ExternalSemaphorePool> pool,
         std::vector<ExternalSemaphore> semaphores,
         VulkanDeviceQueue* device_queue, bool device_lost) {
        if (pool)
          pool->ReturnSemaphores(std::move(semaphores));
      },
      weak_ptr_factory_.GetWeakPtr(), std::move(semaphores)));
}

}  // namespace gpu
