// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"

#include <limits>
#include <utility>

#include "base/trace_event/trace_event.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"

namespace gpu {

ExternalVkImageSkiaRepresentation::ExternalVkImageSkiaRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker)
    : SharedImageRepresentationSkia(manager, backing, tracker) {
}

ExternalVkImageSkiaRepresentation::~ExternalVkImageSkiaRepresentation() {
  DCHECK_EQ(access_mode_, kNone) << "Previoud access hasn't end yet.";
  DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);
  backing_impl()->context_state()->EraseCachedSkSurface(this);
}

sk_sp<SkSurface> ExternalVkImageSkiaRepresentation::BeginWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  auto* gr_context = backing_impl()->context_state()->gr_context();
  if (gr_context->abandoned()) {
    LOG(ERROR) << "GrContext is abandonded.";
    return nullptr;
  }

  if (access_mode_ != kNone) {
    LOG(DFATAL) << "Previous access hasn't ended yet. mode=" << access_mode_;
    return nullptr;
  }

  auto promise_texture =
      BeginAccess(false /* readonly */, begin_semaphores, end_semaphores);
  if (!promise_texture) {
    LOG(ERROR) << "BeginAccess failed";
    return nullptr;
  }

  auto surface = backing_impl()->context_state()->GetCachedSkSurface(this);

  // If surface properties are different from the last access, then we cannot
  // reuse the cached SkSurface.
  if (!surface || surface_props != surface->props() ||
      final_msaa_count != surface_msaa_count_) {
    SkColorType sk_color_type = viz::ResourceFormatToClosestSkColorType(
        true /* gpu_compositing */, format());
    surface = SkSurface::MakeFromBackendTextureAsRenderTarget(
        gr_context, promise_texture->backendTexture(), kTopLeft_GrSurfaceOrigin,
        final_msaa_count, sk_color_type,
        backing_impl()->color_space().ToSkColorSpace(), &surface_props);
    if (!surface) {
      LOG(ERROR) << "MakeFromBackendTextureAsRenderTarget() failed.";
      backing_impl()->context_state()->EraseCachedSkSurface(this);
      return nullptr;
    }
    surface_msaa_count_ = final_msaa_count;
    backing_impl()->context_state()->CacheSkSurface(this, surface);
  }

  int count = surface->getCanvas()->save();
  DCHECK_EQ(count, 1);
  ALLOW_UNUSED_LOCAL(count);

  access_mode_ = kWrite;
  return surface;
}

void ExternalVkImageSkiaRepresentation::EndWriteAccess(
    sk_sp<SkSurface> surface) {
  if (access_mode_ != kWrite) {
    LOG(DFATAL) << "BeginWriteAccess is not called mode=" << access_mode_;
    return;
  }
  surface->getCanvas()->restoreToCount(1);
  surface = nullptr;
  DCHECK(backing_impl()->context_state()->CachedSkSurfaceIsUnique(this));
  EndAccess(false /* readonly */);
  access_mode_ = kNone;
}

sk_sp<SkPromiseImageTexture> ExternalVkImageSkiaRepresentation::BeginReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (access_mode_ != kNone) {
    LOG(DFATAL) << "Previous access is not finished. mode=" << access_mode_;
    return nullptr;
  }

  auto promise_texture =
      BeginAccess(true /* readonly */, begin_semaphores, end_semaphores);
  if (!promise_texture) {
    LOG(ERROR) << "BeginAccess failed";
    return nullptr;
  }
  access_mode_ = kRead;
  return promise_texture;
}

void ExternalVkImageSkiaRepresentation::EndReadAccess() {
  if (access_mode_ != kRead) {
    LOG(DFATAL) << "BeginReadAccess is not called. mode=" << access_mode_;
    return;
  }

  EndAccess(true /* readonly */);
  access_mode_ = kNone;
}

sk_sp<SkPromiseImageTexture> ExternalVkImageSkiaRepresentation::BeginAccess(
    bool readonly,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  DCHECK_EQ(access_mode_, kNone);
  DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);

  std::vector<SemaphoreHandle> handles;
  if (!backing_impl()->BeginAccess(readonly, &handles, false /* is_gl */))
    return nullptr;

  for (auto& handle : handles) {
    DCHECK(handle.is_valid());
    VkSemaphore semaphore = vk_implementation()->ImportSemaphoreHandle(
        vk_device(), std::move(handle));
    DCHECK(semaphore != VK_NULL_HANDLE);
    // The ownership of semaphore is passed to caller.
    begin_semaphores->emplace_back();
    begin_semaphores->back().initVulkan(semaphore);
  }

  if (backing_impl()->need_synchronization()) {
    // Create an |end_access_semaphore_| which will be signalled by the caller.
    end_access_semaphore_ =
        vk_implementation()->CreateExternalSemaphore(backing_impl()->device());
    DCHECK(end_access_semaphore_ != VK_NULL_HANDLE);
    end_semaphores->emplace_back();
    end_semaphores->back().initVulkan(end_access_semaphore_);
  }

  return SkPromiseImageTexture::Make(backing_impl()->backend_texture());
}

void ExternalVkImageSkiaRepresentation::EndAccess(bool readonly) {
  DCHECK_NE(access_mode_, kNone);

  SemaphoreHandle handle;
  if (backing_impl()->need_synchronization()) {
    DCHECK(end_access_semaphore_ != VK_NULL_HANDLE);

    handle = vk_implementation()->GetSemaphoreHandle(vk_device(),
                                                     end_access_semaphore_);
    DCHECK(handle.is_valid());

    // We're done with the semaphore, enqueue deferred cleanup.
    fence_helper()->EnqueueSemaphoreCleanupForSubmittedWork(
        end_access_semaphore_);
    end_access_semaphore_ = VK_NULL_HANDLE;
  } else {
    DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);
  }

  backing_impl()->EndAccess(readonly, std::move(handle), false /* is_gl */);
}

}  // namespace gpu
