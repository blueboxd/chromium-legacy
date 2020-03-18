// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_dawn_representation.h"

#include <dawn_native/VulkanBackend.h>

#include <iostream>
#include <utility>
#include <vector>

#include "base/posix/eintr_wrapper.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_instance.h"
#include "ui/gl/buildflags.h"

namespace gpu {

ExternalVkImageDawnRepresentation::ExternalVkImageDawnRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device,
    WGPUTextureFormat wgpu_format,
    base::ScopedFD memory_fd)
    : SharedImageRepresentationDawn(manager, backing, tracker),
      device_(device),
      wgpu_format_(wgpu_format),
      memory_fd_(std::move(memory_fd)),
      dawn_procs_(dawn_native::GetProcs()) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid (it might become
  // lost in which case operations will be noops).
  dawn_procs_.deviceReference(device_);
}

ExternalVkImageDawnRepresentation::~ExternalVkImageDawnRepresentation() {
  EndAccess();
  dawn_procs_.deviceRelease(device_);
}

WGPUTexture ExternalVkImageDawnRepresentation::BeginAccess(
    WGPUTextureUsage usage) {
  std::vector<SemaphoreHandle> handles;

  if (!backing_impl()->BeginAccess(false, &handles, false /* is_gl */)) {
    return nullptr;
  }

  WGPUTextureDescriptor texture_descriptor = {};
  texture_descriptor.nextInChain = nullptr;
  texture_descriptor.format = wgpu_format_;
  texture_descriptor.usage = usage;
  texture_descriptor.dimension = WGPUTextureDimension_2D;
  texture_descriptor.size = {size().width(), size().height(), 1};
  texture_descriptor.arrayLayerCount = 1;
  texture_descriptor.mipLevelCount = 1;
  texture_descriptor.sampleCount = 1;

  dawn_native::vulkan::ExternalImageDescriptorOpaqueFD descriptor = {};
  descriptor.cTextureDescriptor = &texture_descriptor;
  descriptor.isCleared = IsCleared();
  descriptor.allocationSize = backing_impl()->image()->device_size();
  descriptor.memoryTypeIndex = backing_impl()->image()->memory_type_index();
  descriptor.memoryFD = dup(memory_fd_.get());

  // TODO(http://crbug.com/dawn/200): We may not be obeying all of the rules
  // specified by Vulkan for external queue transfer barriers. Investigate this.

  // Take ownership of file descriptors and transfer to dawn
  for (SemaphoreHandle& handle : handles) {
    descriptor.waitFDs.push_back(handle.TakeHandle().release());
  }

  texture_ = dawn_native::vulkan::WrapVulkanImage(device_, &descriptor);

  if (texture_) {
    // Keep a reference to the texture so that it stays valid (its content
    // might be destroyed).
    dawn_procs_.textureReference(texture_);
  }

  return texture_;
}

void ExternalVkImageDawnRepresentation::EndAccess() {
  if (!texture_) {
    return;
  }

  // Grab the signal semaphore from dawn
  int signal_semaphore_fd =
      dawn_native::vulkan::ExportSignalSemaphoreOpaqueFD(device_, texture_);

  if (dawn_native::IsTextureSubresourceInitialized(texture_, 0, 1, 0, 1)) {
    SetCleared();
  }

  // Wrap file descriptor in a handle
  SemaphoreHandle signal_semaphore(
      VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT_KHR,
      base::ScopedFD(signal_semaphore_fd));

  backing_impl()->EndAccess(false, std::move(signal_semaphore),
                            false /* is_gl */);

  // Destroy the texture, signaling the semaphore in dawn
  dawn_procs_.textureDestroy(texture_);
  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;
}

}  // namespace gpu
