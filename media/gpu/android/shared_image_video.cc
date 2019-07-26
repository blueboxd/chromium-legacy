// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/shared_image_video.h"

#include <utility>

#include "base/android/scoped_hardware_buffer_fence_sync.h"
#include "base/android/scoped_hardware_buffer_handle.h"
#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/service/abstract_texture.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/shared_context_state.h"
#include "gpu/command_buffer/service/shared_image_representation.h"
#include "gpu/command_buffer/service/shared_image_representation_skia_gl.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_implementation.h"
#include "gpu/vulkan/vulkan_util.h"
#include "media/gpu/android/codec_image.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"
#include "third_party/skia/include/gpu/GrBackendSemaphore.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"

namespace media {

namespace {
sk_sp<SkPromiseImageTexture> CreatePromiseTexture(
    viz::VulkanContextProvider* context_provider,
    base::android::ScopedHardwareBufferHandle ahb_handle,
    gfx::Size size,
    viz::ResourceFormat format) {
  gpu::VulkanImplementation* vk_implementation =
      context_provider->GetVulkanImplementation();
  VkDevice vk_device = context_provider->GetDeviceQueue()->GetVulkanDevice();
  VkPhysicalDevice vk_physical_device =
      context_provider->GetDeviceQueue()->GetVulkanPhysicalDevice();

  // Create a VkImage and import AHB.
  VkImage vk_image;
  VkImageCreateInfo vk_image_info;
  VkDeviceMemory vk_device_memory;
  VkDeviceSize mem_allocation_size;
  gpu::VulkanYCbCrInfo ycbcr_info;
  if (!vk_implementation->CreateVkImageAndImportAHB(
          vk_device, vk_physical_device, size, std::move(ahb_handle), &vk_image,
          &vk_image_info, &vk_device_memory, &mem_allocation_size,
          &ycbcr_info)) {
    return nullptr;
  }

  GrVkYcbcrConversionInfo fYcbcrConversionInfo(
      static_cast<VkSamplerYcbcrModelConversion>(
          ycbcr_info.suggested_ycbcr_model),
      static_cast<VkSamplerYcbcrRange>(ycbcr_info.suggested_ycbcr_range),
      static_cast<VkChromaLocation>(ycbcr_info.suggested_xchroma_offset),
      static_cast<VkChromaLocation>(ycbcr_info.suggested_ychroma_offset),
      VK_FILTER_LINEAR,  // VkFilter
      0,                 // VkBool32 forceExplicitReconstruction
      ycbcr_info.external_format,
      static_cast<VkFormatFeatureFlags>(ycbcr_info.format_features));

  // Create backend texture from the VkImage.
  GrVkAlloc alloc = {vk_device_memory, 0, mem_allocation_size, 0};
  GrVkImageInfo vk_info = {vk_image,
                           alloc,
                           vk_image_info.tiling,
                           vk_image_info.initialLayout,
                           vk_image_info.format,
                           vk_image_info.mipLevels,
                           VK_QUEUE_FAMILY_EXTERNAL,
                           GrProtected::kNo,
                           fYcbcrConversionInfo};

  // TODO(bsalomon): Determine whether it makes sense to attempt to reuse this
  // if the vk_info stays the same on subsequent calls.
  auto promise_texture = SkPromiseImageTexture::Make(
      GrBackendTexture(size.width(), size.height(), vk_info));
  if (!promise_texture) {
    vkDestroyImage(vk_device, vk_image, nullptr);
    vkFreeMemory(vk_device, vk_device_memory, nullptr);
    return nullptr;
  }

  return promise_texture;
}

void DestroyVkPromiseTexture(viz::VulkanContextProvider* context_provider,
                             sk_sp<SkPromiseImageTexture> promise_texture) {
  DCHECK(promise_texture);
  DCHECK(promise_texture->unique());

  GrVkImageInfo vk_image_info;
  bool result =
      promise_texture->backendTexture().getVkImageInfo(&vk_image_info);
  DCHECK(result);

  gpu::VulkanFenceHelper* fence_helper =
      context_provider->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueImageCleanupForSubmittedWork(
      vk_image_info.fImage, vk_image_info.fAlloc.fMemory);
}

}  // namespace

SharedImageVideo::SharedImageVideo(
    const gpu::Mailbox& mailbox,
    const gfx::ColorSpace color_space,
    scoped_refptr<CodecImage> codec_image,
    std::unique_ptr<gpu::gles2::AbstractTexture> abstract_texture,
    scoped_refptr<gpu::SharedContextState> context_state,
    bool is_thread_safe)
    : SharedImageBacking(
          mailbox,
          viz::RGBA_8888,
          codec_image->GetSize(),
          color_space,
          (gpu::SHARED_IMAGE_USAGE_DISPLAY | gpu::SHARED_IMAGE_USAGE_GLES2),
          viz::ResourceSizes::UncheckedSizeInBytes<size_t>(
              codec_image->GetSize(),
              viz::RGBA_8888),
          is_thread_safe),
      codec_image_(std::move(codec_image)),
      abstract_texture_(std::move(abstract_texture)),
      context_state_(std::move(context_state)) {
  DCHECK(codec_image_);
  DCHECK(context_state_);

  // Currently this backing is not thread safe.
  DCHECK(!is_thread_safe);
  context_state_->AddContextLostObserver(this);
}

SharedImageVideo::~SharedImageVideo() {
  codec_image_->ReleaseCodecBuffer();
  if (context_state_)
    context_state_->RemoveContextLostObserver(this);
}

bool SharedImageVideo::IsCleared() const {
  return true;
}

void SharedImageVideo::SetCleared() {}

void SharedImageVideo::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
}

bool SharedImageVideo::ProduceLegacyMailbox(
    gpu::MailboxManager* mailbox_manager) {
  DCHECK(abstract_texture_);
  mailbox_manager->ProduceTexture(mailbox(),
                                  abstract_texture_->GetTextureBase());
  return true;
}

void SharedImageVideo::Destroy() {}

size_t SharedImageVideo::EstimatedSizeForMemTracking() const {
  // This backing contributes to gpu memory only if its bound to the texture and
  // not when the backing is created.
  return codec_image_->was_tex_image_bound() ? estimated_size() : 0;
}

void SharedImageVideo::OnContextLost() {
  // We release codec buffers when shared image context is lost. This is because
  // texture owner's texture was created on shared context. Once shared context
  // is lost, no one should try to use that texture.
  codec_image_->ReleaseCodecBuffer();
  context_state_->RemoveContextLostObserver(this);
  context_state_ = nullptr;
}

base::Optional<gpu::VulkanYCbCrInfo> SharedImageVideo::GetYcbcrInfo() {
  // For non-vulkan context, return null.
  if (!context_state_->GrContextIsVulkan())
    return base::nullopt;

  // Render the codec image.
  codec_image_->RenderToFrontBuffer();

  // Get the AHB from the latest image.
  auto scoped_hardware_buffer =
      codec_image_->texture_owner()->GetAHardwareBuffer();
  if (!scoped_hardware_buffer) {
    return base::nullopt;
  }

  DCHECK(scoped_hardware_buffer->buffer());
  auto* context_provider = context_state_->vk_context_provider();
  gpu::VulkanImplementation* vk_implementation =
      context_provider->GetVulkanImplementation();
  VkDevice vk_device = context_provider->GetDeviceQueue()->GetVulkanDevice();

  gpu::VulkanYCbCrInfo ycbcr_info;
  if (!vk_implementation->GetSamplerYcbcrConversionInfo(
          vk_device, scoped_hardware_buffer->TakeBuffer(), &ycbcr_info)) {
    LOG(ERROR) << "Failed to get the ycbcr info.";
    return base::nullopt;
  }
  return base::Optional<gpu::VulkanYCbCrInfo>(ycbcr_info);
}

// Representation of SharedImageVideo as a GL Texture.
class SharedImageRepresentationGLTextureVideo
    : public gpu::SharedImageRepresentationGLTexture {
 public:
  SharedImageRepresentationGLTextureVideo(gpu::SharedImageManager* manager,
                                          SharedImageVideo* backing,
                                          gpu::MemoryTypeTracker* tracker,
                                          gpu::gles2::Texture* texture)
      : gpu::SharedImageRepresentationGLTexture(manager, backing, tracker),
        texture_(texture) {}

  gpu::gles2::Texture* GetTexture() override { return texture_; }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK_EQ(mode,
              static_cast<GLenum>(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));

    auto* video_backing = static_cast<SharedImageVideo*>(backing());
    video_backing->BeginGLReadAccess();
    return true;
  }

  void EndAccess() override {}

 private:
  gpu::gles2::Texture* texture_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLTextureVideo);
};

// Representation of SharedImageVideo as a GL Texture.
class SharedImageRepresentationGLTexturePassthroughVideo
    : public gpu::SharedImageRepresentationGLTexturePassthrough {
 public:
  SharedImageRepresentationGLTexturePassthroughVideo(
      gpu::SharedImageManager* manager,
      SharedImageVideo* backing,
      gpu::MemoryTypeTracker* tracker,
      scoped_refptr<gpu::gles2::TexturePassthrough> texture)
      : gpu::SharedImageRepresentationGLTexturePassthrough(manager,
                                                           backing,
                                                           tracker),
        texture_(std::move(texture)) {}

  const scoped_refptr<gpu::gles2::TexturePassthrough>& GetTexturePassthrough()
      override {
    return texture_;
  }

  bool BeginAccess(GLenum mode) override {
    // This representation should only be called for read.
    DCHECK_EQ(mode,
              static_cast<GLenum>(GL_SHARED_IMAGE_ACCESS_MODE_READ_CHROMIUM));

    auto* video_backing = static_cast<SharedImageVideo*>(backing());
    video_backing->BeginGLReadAccess();
    return true;
  }

  void EndAccess() override {}

 private:
  scoped_refptr<gpu::gles2::TexturePassthrough> texture_;

  DISALLOW_COPY_AND_ASSIGN(SharedImageRepresentationGLTexturePassthroughVideo);
};

// Vulkan backed Skia representation of SharedImageVideo.
class SharedImageRepresentationVideoSkiaVk
    : public gpu::SharedImageRepresentationSkia {
 public:
  SharedImageRepresentationVideoSkiaVk(
      gpu::SharedImageManager* manager,
      gpu::SharedImageBacking* backing,
      scoped_refptr<gpu::SharedContextState> context_state,
      gpu::MemoryTypeTracker* tracker)
      : gpu::SharedImageRepresentationSkia(manager, backing, tracker),
        context_state_(std::move(context_state)) {
    DCHECK(context_state_);
    DCHECK(context_state_->vk_context_provider());
  }

  ~SharedImageRepresentationVideoSkiaVk() override {
    DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);

    // |promise_texture_| could be null if we never being read.
    if (!promise_texture_)
      return;
    DestroyVkPromiseTexture(context_state_->vk_context_provider(),
                            std::move(promise_texture_));
  }

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    // Writes are not intended to used for video backed representations.
    NOTIMPLEMENTED();
    return nullptr;
  }

  void EndWriteAccess(sk_sp<SkSurface> surface) override { NOTIMPLEMENTED(); }

  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override {
    if (!scoped_hardware_buffer_) {
      auto* video_backing = static_cast<SharedImageVideo*>(backing());
      DCHECK(video_backing);
      auto* codec_image = video_backing->codec_image_.get();
      auto* texture_owner = codec_image->texture_owner().get();

      // Render the codec image and get AHB from latest image.
      codec_image->RenderToFrontBuffer();
      scoped_hardware_buffer_ = texture_owner->GetAHardwareBuffer();
      if (!scoped_hardware_buffer_) {
        LOG(ERROR) << "Failed to get the hardware buffer.";
        return nullptr;
      }
    }
    DCHECK(scoped_hardware_buffer_->buffer());

    // Wait on the sync fd attached to the buffer to make sure buffer is
    // ready before the read. This is done by inserting the sync fd semaphore
    // into begin_semaphore vector which client will wait on.
    base::ScopedFD sync_fd = scoped_hardware_buffer_->TakeFence();
    if (!BeginRead(begin_semaphores, end_semaphores, std::move(sync_fd))) {
      return nullptr;
    }

    if (!promise_texture_) {
      // Create the promise texture.
      promise_texture_ = CreatePromiseTexture(
          context_state_->vk_context_provider(),
          scoped_hardware_buffer_->TakeBuffer(), size(), format());
    }
    return promise_texture_;
  }

  void EndReadAccess() override {
    DCHECK(end_access_semaphore_ != VK_NULL_HANDLE);

    gpu::SemaphoreHandle semaphore_handle =
        vk_implementation()->GetSemaphoreHandle(vk_device(),
                                                end_access_semaphore_);
    auto sync_fd = semaphore_handle.TakeHandle();
    DCHECK(sync_fd.is_valid());

    // Pass the end access sync fd to the scoped hardware buffer. This will make
    // sure that the AImage associated with the hardware buffer will be deleted
    // only when the read access is ending.
    scoped_hardware_buffer_->SetReadFence(std::move(sync_fd), true);
    fence_helper()->EnqueueSemaphoreCleanupForSubmittedWork(
        end_access_semaphore_);
    end_access_semaphore_ = VK_NULL_HANDLE;
  }

 private:
  bool BeginRead(std::vector<GrBackendSemaphore>* begin_semaphores,
                 std::vector<GrBackendSemaphore>* end_semaphores,
                 base::ScopedFD sync_fd) {
    DCHECK(begin_semaphores);
    DCHECK(end_semaphores);
    DCHECK(end_access_semaphore_ == VK_NULL_HANDLE);

    VkSemaphore begin_access_semaphore = VK_NULL_HANDLE;
    if (sync_fd.is_valid()) {
      begin_access_semaphore = vk_implementation()->ImportSemaphoreHandle(
          vk_device(),
          gpu::SemaphoreHandle(VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_SYNC_FD_BIT,
                               std::move(sync_fd)));
      if (begin_access_semaphore == VK_NULL_HANDLE) {
        DLOG(ERROR) << "Failed to import semaphore from sync_fd.";
        return false;
      }
    }

    end_access_semaphore_ =
        vk_implementation()->CreateExternalSemaphore(vk_device());

    if (end_access_semaphore_ == VK_NULL_HANDLE) {
      DLOG(ERROR) << "Failed to create the external semaphore.";
      if (begin_access_semaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(vk_device(), begin_access_semaphore,
                           nullptr /* pAllocator */);
      }
      return false;
    }
    end_semaphores->emplace_back();
    end_semaphores->back().initVulkan(end_access_semaphore_);

    if (begin_access_semaphore != VK_NULL_HANDLE) {
      begin_semaphores->emplace_back();
      begin_semaphores->back().initVulkan(begin_access_semaphore);
    }
    return true;
  }

  VkDevice vk_device() {
    return context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetVulkanDevice();
  }

  gpu::VulkanImplementation* vk_implementation() {
    return context_state_->vk_context_provider()->GetVulkanImplementation();
  }

  gpu::VulkanFenceHelper* fence_helper() {
    return context_state_->vk_context_provider()
        ->GetDeviceQueue()
        ->GetFenceHelper();
  }

  sk_sp<SkPromiseImageTexture> promise_texture_;
  scoped_refptr<gpu::SharedContextState> context_state_;
  std::unique_ptr<base::android::ScopedHardwareBufferFenceSync>
      scoped_hardware_buffer_;
  VkSemaphore end_access_semaphore_ = VK_NULL_HANDLE;
};

// TODO(vikassoni): Currently GLRenderer doesn't support overlays with shared
// image. Add support for overlays in GLRenderer as well as overlay
// representations of shared image.
std::unique_ptr<gpu::SharedImageRepresentationGLTexture>
SharedImageVideo::ProduceGLTexture(gpu::SharedImageManager* manager,
                                   gpu::MemoryTypeTracker* tracker) {
  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!codec_image_->texture_owner())
    return nullptr;
  // TODO(vikassoni): We would want to give the TextureOwner's underlying
  // Texture, but it was not set with the correct size. The AbstractTexture,
  // that we use for legacy mailbox, is correctly set.
  auto* texture =
      gpu::gles2::Texture::CheckedCast(abstract_texture_->GetTextureBase());
  DCHECK(texture);

  return std::make_unique<SharedImageRepresentationGLTextureVideo>(
      manager, this, tracker, texture);
}

// TODO(vikassoni): Currently GLRenderer doesn't support overlays with shared
// image. Add support for overlays in GLRenderer as well as overlay
// representations of shared image.
std::unique_ptr<gpu::SharedImageRepresentationGLTexturePassthrough>
SharedImageVideo::ProduceGLTexturePassthrough(gpu::SharedImageManager* manager,
                                              gpu::MemoryTypeTracker* tracker) {
  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!codec_image_->texture_owner())
    return nullptr;
  // TODO(vikassoni): We would want to give the TextureOwner's underlying
  // Texture, but it was not set with the correct size. The AbstractTexture,
  // that we use for legacy mailbox, is correctly set.
  scoped_refptr<gpu::gles2::TexturePassthrough> texture =
      gpu::gles2::TexturePassthrough::CheckedCast(
          abstract_texture_->GetTextureBase());
  DCHECK(texture);

  return std::make_unique<SharedImageRepresentationGLTexturePassthroughVideo>(
      manager, this, tracker, std::move(texture));
}

// Currently SkiaRenderer doesn't support overlays.
std::unique_ptr<gpu::SharedImageRepresentationSkia>
SharedImageVideo::ProduceSkia(
    gpu::SharedImageManager* manager,
    gpu::MemoryTypeTracker* tracker,
    scoped_refptr<gpu::SharedContextState> context_state) {
  DCHECK(context_state);

  // For (old) overlays, we don't have a texture owner, but overlay promotion
  // might not happen for some reasons. In that case, it will try to draw
  // which should result in no image.
  if (!codec_image_->texture_owner())
    return nullptr;

  if (context_state->GrContextIsVulkan()) {
    return std::make_unique<SharedImageRepresentationVideoSkiaVk>(
        manager, this, std::move(context_state), tracker);
  }

  DCHECK(context_state->GrContextIsGL());
  auto* texture = gpu::gles2::Texture::CheckedCast(
      codec_image_->texture_owner()->GetTextureBase());
  DCHECK(texture);

  // In GL mode, create the SharedImageRepresentationGLTextureVideo
  // representation to use with SharedImageRepresentationVideoSkiaGL.
  auto gl_representation =
      std::make_unique<SharedImageRepresentationGLTextureVideo>(
          manager, this, tracker, texture);
  return gpu::SharedImageRepresentationSkiaGL::Create(
      std::move(gl_representation), nullptr, manager, this, tracker);
}

void SharedImageVideo::BeginGLReadAccess() {
  // Render the codec image.
  codec_image_->RenderToFrontBuffer();

  // Bind the tex image if it's not already bound.
  auto* texture_owner = codec_image_->texture_owner().get();
  if (!texture_owner->binds_texture_on_update())
    texture_owner->EnsureTexImageBound();
}

}  // namespace media
