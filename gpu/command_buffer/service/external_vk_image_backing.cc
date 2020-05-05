// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/external_vk_image_backing.h"

#include <utility>
#include <vector>

#include "base/stl_util.h"
#include "base/system/sys_info.h"
#include "build/build_config.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "gpu/command_buffer/service/external_vk_image_gl_representation.h"
#include "gpu/command_buffer/service/external_vk_image_skia_representation.h"
#include "gpu/command_buffer/service/skia_utils.h"
#include "gpu/ipc/common/vulkan_ycbcr_info.h"
#include "gpu/vulkan/vma_wrapper.h"
#include "gpu/vulkan/vulkan_command_buffer.h"
#include "gpu/vulkan/vulkan_command_pool.h"
#include "gpu/vulkan/vulkan_fence_helper.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "gpu/vulkan/vulkan_image.h"
#include "gpu/vulkan/vulkan_util.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gl/buildflags.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_version_info.h"
#include "ui/gl/scoped_binders.h"

#if defined(OS_LINUX) && BUILDFLAG(USE_DAWN)
#include "gpu/command_buffer/service/external_vk_image_dawn_representation.h"
#endif

#if defined(OS_FUCHSIA)
#include "gpu/vulkan/fuchsia/vulkan_fuchsia_ext.h"
#endif

#define GL_DEDICATED_MEMORY_OBJECT_EXT 0x9581
#define GL_TEXTURE_TILING_EXT 0x9580
#define GL_TILING_TYPES_EXT 0x9583
#define GL_OPTIMAL_TILING_EXT 0x9584
#define GL_LINEAR_TILING_EXT 0x9585
#define GL_HANDLE_TYPE_OPAQUE_FD_EXT 0x9586
#define GL_HANDLE_TYPE_OPAQUE_WIN32_EXT 0x9587
#define GL_HANDLE_TYPE_ZIRCON_VMO_ANGLE 0x93AE
#define GL_HANDLE_TYPE_ZIRCON_EVENT_ANGLE 0x93AF

namespace gpu {

namespace {

static const struct {
  GLenum gl_format;
  GLenum gl_type;
  GLuint bytes_per_pixel;
} kFormatTable[] = {
    {GL_RGBA, GL_UNSIGNED_BYTE, 4},                // RGBA_8888
    {GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4, 2},       // RGBA_4444
    {GL_BGRA, GL_UNSIGNED_BYTE, 4},                // BGRA_8888
    {GL_RED, GL_UNSIGNED_BYTE, 1},                 // ALPHA_8
    {GL_RED, GL_UNSIGNED_BYTE, 1},                 // LUMINANCE_8
    {GL_RGB, GL_UNSIGNED_SHORT_5_6_5, 2},          // RGB_565
    {GL_BGR, GL_UNSIGNED_SHORT_5_6_5, 2},          // BGR_565
    {GL_ZERO, GL_ZERO, 0},                         // ETC1
    {GL_RED, GL_UNSIGNED_BYTE, 1},                 // RED_8
    {GL_RG, GL_UNSIGNED_BYTE, 2},                  // RG_88
    {GL_RED, GL_HALF_FLOAT_OES, 2},                // LUMINANCE_F16
    {GL_RGBA, GL_HALF_FLOAT_OES, 8},               // RGBA_F16
    {GL_RED, GL_UNSIGNED_SHORT, 2},                // R16_EXT
    {GL_RGBA, GL_UNSIGNED_BYTE, 4},                // RGBX_8888
    {GL_BGRA, GL_UNSIGNED_BYTE, 4},                // BGRX_8888
    {GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV, 4},  // RGBA_1010102
    {GL_BGRA, GL_UNSIGNED_INT_2_10_10_10_REV, 4},  // BGRA_1010102
    {GL_ZERO, GL_ZERO, 0},                         // YVU_420
    {GL_ZERO, GL_ZERO, 0},                         // YUV_420_BIPLANAR
    {GL_ZERO, GL_ZERO, 0},                         // P010
};
static_assert(base::size(kFormatTable) == (viz::RESOURCE_FORMAT_MAX + 1),
              "kFormatTable does not handle all cases.");

class ScopedPixelStore {
 public:
  ScopedPixelStore(gl::GLApi* api, GLenum name, GLint value)
      : api_(api), name_(name), value_(value) {
    api_->glGetIntegervFn(name_, &old_value_);
    if (value_ != old_value_)
      api->glPixelStoreiFn(name_, value_);
  }
  ~ScopedPixelStore() {
    if (value_ != old_value_)
      api_->glPixelStoreiFn(name_, old_value_);
  }

 private:
  gl::GLApi* const api_;
  const GLenum name_;
  const GLint value_;
  GLint old_value_;

  DISALLOW_COPY_AND_ASSIGN(ScopedPixelStore);
};

class ScopedDedicatedMemoryObject {
 public:
  explicit ScopedDedicatedMemoryObject(gl::GLApi* api) : api_(api) {
    api_->glCreateMemoryObjectsEXTFn(1, &id_);
    int dedicated = GL_TRUE;
    api_->glMemoryObjectParameterivEXTFn(id_, GL_DEDICATED_MEMORY_OBJECT_EXT,
                                         &dedicated);
  }
  ~ScopedDedicatedMemoryObject() { api_->glDeleteMemoryObjectsEXTFn(1, &id_); }

  GLuint id() const { return id_; }

 private:
  gl::GLApi* const api_;
  GLuint id_;
};

}  // namespace

// static
std::unique_ptr<ExternalVkImageBacking> ExternalVkImageBacking::Create(
    SharedContextState* context_state,
    VulkanCommandPool* command_pool,
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    const VulkanImageUsageCache* image_usage_cache,
    base::span<const uint8_t> pixel_data,
    bool using_gmb) {
  bool is_external = context_state->support_vulkan_external_object();
  bool is_transfer_dst = using_gmb || !pixel_data.empty() || !is_external;

  auto* device_queue = context_state->vk_context_provider()->GetDeviceQueue();
  VkFormat vk_format = ToVkFormat(format);
  VkImageUsageFlags vk_usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  if (is_transfer_dst)
    vk_usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  // Requested usage flags must be supported.
  DCHECK_EQ(vk_usage & image_usage_cache->optimal_tiling_usage[format],
            vk_usage);

  if (is_external && (usage & SHARED_IMAGE_USAGE_GLES2)) {
    // Must request all available image usage flags if aliasing GL texture. This
    // is a spec requirement.
    vk_usage |= image_usage_cache->optimal_tiling_usage[format];
  }

  auto* vulkan_implementation =
      context_state->vk_context_provider()->GetVulkanImplementation();
  VkImageCreateFlags vk_flags =
      vulkan_implementation->enforce_protected_memory()
          ? VK_IMAGE_CREATE_PROTECTED_BIT
          : 0;
  std::unique_ptr<VulkanImage> image;
  if (is_external) {
    image = VulkanImage::CreateWithExternalMemory(device_queue, size, vk_format,
                                                  vk_usage, vk_flags,
                                                  VK_IMAGE_TILING_OPTIMAL);
  } else {
    image = VulkanImage::Create(device_queue, size, vk_format, vk_usage,
                                vk_flags, VK_IMAGE_TILING_OPTIMAL);
  }
  if (!image)
    return nullptr;

  auto backing = std::make_unique<ExternalVkImageBacking>(
      util::PassKey<ExternalVkImageBacking>(), mailbox, format, size,
      color_space, usage, context_state, std::move(image), command_pool);

  if (!pixel_data.empty()) {
    backing->WritePixels(
        pixel_data.size(), 0,
        base::BindOnce([](const void* data, size_t size,
                          void* buffer) { memcpy(buffer, data, size); },
                       pixel_data.data(), pixel_data.size()));
  }

  return backing;
}

// static
std::unique_ptr<ExternalVkImageBacking> ExternalVkImageBacking::CreateFromGMB(
    SharedContextState* context_state,
    VulkanCommandPool* command_pool,
    const Mailbox& mailbox,
    gfx::GpuMemoryBufferHandle handle,
    gfx::BufferFormat buffer_format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    const VulkanImageUsageCache* image_usage_cache) {
  if (!gpu::IsImageSizeValidForGpuMemoryBufferFormat(size, buffer_format)) {
    DLOG(ERROR) << "Invalid image size for format.";
    return nullptr;
  }

  auto* vulkan_implementation =
      context_state->vk_context_provider()->GetVulkanImplementation();
  auto resource_format = viz::GetResourceFormat(buffer_format);
  if (vulkan_implementation->CanImportGpuMemoryBuffer(handle.type)) {
    auto* device_queue = context_state->vk_context_provider()->GetDeviceQueue();
    VkFormat vk_format = ToVkFormat(resource_format);
    auto image = vulkan_implementation->CreateImageFromGpuMemoryHandle(
        device_queue, std::move(handle), size, vk_format);
    if (!image) {
      DLOG(ERROR) << "Failed to create VkImage from GpuMemoryHandle.";
      return nullptr;
    }

    auto backing = std::make_unique<ExternalVkImageBacking>(
        util::PassKey<ExternalVkImageBacking>(), mailbox, resource_format, size,
        color_space, usage, context_state, std::move(image), command_pool);
    backing->SetCleared();
    return backing;
  }

  if (gfx::NumberOfPlanesForLinearBufferFormat(buffer_format) != 1) {
    DLOG(ERROR) << "Invalid image format.";
    return nullptr;
  }

  DCHECK_EQ(handle.type, gfx::SHARED_MEMORY_BUFFER);
  if (!base::IsValueInRangeForNumericType<size_t>(handle.stride))
    return nullptr;

  int32_t width_in_bytes = 0;
  if (!viz::ResourceSizes::MaybeWidthInBytes(size.width(), resource_format,
                                             &width_in_bytes)) {
    DLOG(ERROR) << "ResourceSizes::MaybeWidthInBytes() failed.";
    return nullptr;
  }

  if (handle.stride < width_in_bytes) {
    DLOG(ERROR) << "Invalid GMB stride.";
    return nullptr;
  }

  auto bits_per_pixel = viz::BitsPerPixel(resource_format);
  switch (bits_per_pixel) {
    case 64:
    case 32:
    case 16:
      if (handle.stride % (bits_per_pixel / 8) != 0) {
        DLOG(ERROR) << "Invalid GMB stride.";
        return nullptr;
      }
      break;
    case 8:
    case 4:
      break;
    case 12:
      // We are not supporting YVU420 and YUV_420_BIPLANAR format.
    default:
      NOTREACHED();
      return nullptr;
  }

  if (!handle.region.IsValid()) {
    DLOG(ERROR) << "Invalid GMB shared memory region.";
    return nullptr;
  }

  base::CheckedNumeric<size_t> checked_size = handle.stride;
  checked_size *= size.height();
  if (!checked_size.IsValid()) {
    DLOG(ERROR) << "Invalid GMB size.";
    return nullptr;
  }

  // Minimize the amount of address space we use but make sure offset is a
  // multiple of page size as required by MapAt().
  size_t memory_offset =
      handle.offset % base::SysInfo::VMAllocationGranularity();
  size_t map_offset =
      base::SysInfo::VMAllocationGranularity() *
      (handle.offset / base::SysInfo::VMAllocationGranularity());
  checked_size += memory_offset;
  if (!checked_size.IsValid()) {
    DLOG(ERROR) << "Invalid GMB size.";
    return nullptr;
  }

  auto shared_memory_mapping = handle.region.MapAt(
      static_cast<off_t>(map_offset), checked_size.ValueOrDie());

  if (!shared_memory_mapping.IsValid()) {
    DLOG(ERROR) << "Failed to map shared memory.";
    return nullptr;
  }

  auto backing = Create(context_state, command_pool, mailbox, resource_format,
                        size, color_space, usage, image_usage_cache,
                        base::span<const uint8_t>(), true /* using_gmb */);
  if (!backing)
    return nullptr;

  backing->InstallSharedMemory(std::move(shared_memory_mapping), handle.stride,
                               memory_offset);
  return backing;
}

ExternalVkImageBacking::ExternalVkImageBacking(
    util::PassKey<ExternalVkImageBacking>,
    const Mailbox& mailbox,
    viz::ResourceFormat format,
    const gfx::Size& size,
    const gfx::ColorSpace& color_space,
    uint32_t usage,
    SharedContextState* context_state,
    std::unique_ptr<VulkanImage> image,
    VulkanCommandPool* command_pool)
    : ClearTrackingSharedImageBacking(mailbox,
                                      format,
                                      size,
                                      color_space,
                                      usage,
                                      image->device_size(),
                                      false /* is_thread_safe */),
      context_state_(context_state),
      image_(std::move(image)),
      backend_texture_(size.width(),
                       size.height(),
                       CreateGrVkImageInfo(image_.get())),
      command_pool_(command_pool) {}

ExternalVkImageBacking::~ExternalVkImageBacking() {
  GrVkImageInfo image_info;
  bool result = backend_texture_.getVkImageInfo(&image_info);
  DCHECK(result);

  auto* fence_helper = context_state()
                           ->vk_context_provider()
                           ->GetDeviceQueue()
                           ->GetFenceHelper();
  fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(std::move(image_));
  backend_texture_ = GrBackendTexture();

  if (texture_) {
    // Ensure that a context is current before removing the ref and calling
    // glDeleteTextures.
    if (!gl::GLContext::GetCurrent())
      context_state()->MakeCurrent(nullptr, true /* need_gl */);
    texture_->RemoveLightweightRef(have_context());
  }

  if (texture_passthrough_) {
    // Ensure that a context is current before releasing |texture_passthrough_|,
    // it calls glDeleteTextures.
    if (!gl::GLContext::GetCurrent())
      context_state()->MakeCurrent(nullptr, true /* need_gl */);
    if (!have_context())
      texture_passthrough_->MarkContextLost();
    texture_passthrough_ = nullptr;
  }
}

bool ExternalVkImageBacking::BeginAccess(
    bool readonly,
    std::vector<SemaphoreHandle>* semaphore_handles,
    bool is_gl) {
  if (readonly && !reads_in_progress_) {
    UpdateContent(kInVkImage);
    if (texture_)
      UpdateContent(kInGLTexture);
  }
  if (!BeginAccessInternal(readonly, semaphore_handles))
    return false;

  if (!is_gl)
    return true;

  if (use_separate_gl_texture())
    return true;

  DCHECK(need_synchronization());

  auto command_buffer = command_pool_->CreatePrimaryCommandBuffer();
  {
    ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);
    GrVkImageInfo image_info;
    bool success = backend_texture_.getVkImageInfo(&image_info);
    DCHECK(success);
    auto image_layout = image_info.fImageLayout;
    if (image_layout == VK_IMAGE_LAYOUT_UNDEFINED) {
      // dst_image_layout cannot be VK_IMAGE_LAYOUT_UNDEFINED, so we set it to
      // VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL.
      image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      command_buffer->TransitionImageLayout(
          image_info.fImage, image_info.fImageLayout, image_layout);
      // Update backend_texture_ image layout.
      backend_texture_.setVkImageLayout(image_layout);
    }
    uint32_t vulkan_queue_index = context_state_->vk_context_provider()
                                      ->GetDeviceQueue()
                                      ->GetVulkanQueueIndex();
    // Transfer image queue faimily ownership to external, so the image can be
    // used by GL.
    command_buffer->TransitionImageLayout(image_info.fImage, image_layout,
                                          image_layout, vulkan_queue_index,
                                          VK_QUEUE_FAMILY_EXTERNAL);
  }

  std::vector<VkSemaphore> wait_semaphores;
  wait_semaphores.reserve(semaphore_handles->size());
  for (auto& handle : *semaphore_handles) {
    VkSemaphore semaphore = vulkan_implementation()->ImportSemaphoreHandle(
        device(), std::move(handle));
    wait_semaphores.emplace_back(semaphore);
  }
  semaphore_handles->clear();

  VkSemaphore signal_semaphore =
      vulkan_implementation()->CreateExternalSemaphore(device());
  // TODO(penghuang): ask skia to do it for us to avoid this queue submission.
  command_buffer->Submit(wait_semaphores.size(), wait_semaphores.data(), 1,
                         &signal_semaphore);
  auto end_access_semphore_handle =
      vulkan_implementation()->GetSemaphoreHandle(device(), signal_semaphore);
  semaphore_handles->push_back(std::move(end_access_semphore_handle));

  auto* fence_helper =
      context_state_->vk_context_provider()->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
      std::move(command_buffer));
  wait_semaphores.emplace_back(signal_semaphore);
  fence_helper->EnqueueSemaphoresCleanupForSubmittedWork(
      std::move(wait_semaphores));

  return true;
}

void ExternalVkImageBacking::EndAccess(bool readonly,
                                       SemaphoreHandle semaphore_handle,
                                       bool is_gl) {
  if (is_gl && !use_separate_gl_texture()) {
    auto command_buffer = command_pool_->CreatePrimaryCommandBuffer();
    {
      ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);
      GrVkImageInfo image_info;
      bool success = backend_texture_.getVkImageInfo(&image_info);
      DCHECK(success);
      uint32_t vulkan_queue_index = context_state_->vk_context_provider()
                                        ->GetDeviceQueue()
                                        ->GetVulkanQueueIndex();

      // After GL accessing, transfer image queue family ownership back, so it
      // can be used by vulkan.
      command_buffer->TransitionImageLayout(
          image_info.fImage, image_info.fImageLayout, image_info.fImageLayout,
          VK_QUEUE_FAMILY_EXTERNAL, vulkan_queue_index);
    }

    VkSemaphore semaphore = vulkan_implementation()->ImportSemaphoreHandle(
        device(), std::move(semaphore_handle));
    VkSemaphore end_access_semaphore =
        vulkan_implementation()->CreateExternalSemaphore(device());
    // TODO(penghuang): ask skia to do it for us to avoid this queue submission.
    command_buffer->Submit(1, &semaphore, 1, &end_access_semaphore);
    semaphore_handle = vulkan_implementation()->GetSemaphoreHandle(
        device(), end_access_semaphore);
    auto* fence_helper = context_state_->vk_context_provider()
                             ->GetDeviceQueue()
                             ->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(command_buffer));
    fence_helper->EnqueueSemaphoresCleanupForSubmittedWork(
        {semaphore, end_access_semaphore});
  }

  EndAccessInternal(readonly, std::move(semaphore_handle));
  if (!readonly) {
    if (use_separate_gl_texture()) {
      latest_content_ = is_gl ? kInGLTexture : kInVkImage;
    } else {
      latest_content_ = kInVkImage | kInGLTexture;
    }
  }
}

void ExternalVkImageBacking::Update(std::unique_ptr<gfx::GpuFence> in_fence) {
  DCHECK(!in_fence);
  latest_content_ = kInSharedMemory;
  SetCleared();
}

bool ExternalVkImageBacking::ProduceLegacyMailbox(
    MailboxManager* mailbox_manager) {
  // It is not safe to produce a legacy mailbox because it would bypass the
  // synchronization between Vulkan and GL that is implemented in the
  // representation classes.
  return false;
}

std::unique_ptr<SharedImageRepresentationDawn>
ExternalVkImageBacking::ProduceDawn(SharedImageManager* manager,
                                    MemoryTypeTracker* tracker,
                                    WGPUDevice wgpuDevice) {
#if defined(OS_LINUX) && BUILDFLAG(USE_DAWN)
  auto wgpu_format = viz::ToWGPUFormat(format());

  if (wgpu_format == WGPUTextureFormat_Undefined) {
    DLOG(ERROR) << "Format not supported for Dawn";
    return nullptr;
  }

  GrVkImageInfo image_info;
  bool result = backend_texture_.getVkImageInfo(&image_info);
  DCHECK(result);

  auto memory_fd = image_->GetMemoryFd();
  if (!memory_fd.is_valid()) {
    return nullptr;
  }

  return std::make_unique<ExternalVkImageDawnRepresentation>(
      manager, this, tracker, wgpuDevice, wgpu_format, std::move(memory_fd));
#else  // !defined(OS_LINUX) || !BUILDFLAG(USE_DAWN)
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
#endif
}

GLuint ExternalVkImageBacking::ProduceGLTextureInternal() {
  GrVkImageInfo image_info;
  bool result = backend_texture_.getVkImageInfo(&image_info);
  DCHECK(result);
  gl::GLApi* api = gl::g_current_gl_context;
  base::Optional<ScopedDedicatedMemoryObject> memory_object;
  if (!use_separate_gl_texture()) {
#if defined(OS_LINUX) || defined(OS_ANDROID)
    auto memory_fd = image_->GetMemoryFd();
    if (!memory_fd.is_valid())
      return 0;
    memory_object.emplace(api);
    api->glImportMemoryFdEXTFn(memory_object->id(), image_info.fAlloc.fSize,
                               GL_HANDLE_TYPE_OPAQUE_FD_EXT,
                               memory_fd.release());
#elif defined(OS_WIN)
    auto memory_handle = image_->GetMemoryHandle();
    if (!memory_handle.IsValid()) {
      return 0;
    }
    memory_object.emplace(api);
    api->glImportMemoryWin32HandleEXTFn(
        memory_object->id(), image_info.fAlloc.fSize,
        GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, memory_handle.Take());
#elif defined(OS_FUCHSIA)
    zx::vmo vmo = image_->GetMemoryZirconHandle();
    if (!vmo)
      return 0;
    memory_object.emplace(api);
    api->glImportMemoryZirconHandleANGLEFn(
        memory_object->id(), image_info.fAlloc.fSize,
        GL_HANDLE_TYPE_ZIRCON_VMO_ANGLE, vmo.release());
#else
#error Unsupported OS
#endif
  }

  GLuint internal_format = viz::TextureStorageFormat(format());
  bool is_bgra8 = (internal_format == GL_BGRA8_EXT);
  if (is_bgra8) {
    const auto& ext = gl::g_current_gl_driver->ext;
    if (!ext.b_GL_EXT_texture_format_BGRA8888) {
      bool support_swizzle = ext.b_GL_ARB_texture_swizzle ||
                             ext.b_GL_EXT_texture_swizzle ||
                             gl::g_current_gl_version->IsAtLeastGL(3, 3) ||
                             gl::g_current_gl_version->IsAtLeastGLES(3, 0);
      if (!support_swizzle) {
        LOG(ERROR) << "BGRA_88888 is not supported.";
        return 0;
      }
      internal_format = GL_RGBA8;
    }
  }

  GLuint texture_service_id = 0;
  api->glGenTexturesFn(1, &texture_service_id);
  gl::ScopedTextureBinder scoped_texture_binder(GL_TEXTURE_2D,
                                                texture_service_id);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (use_separate_gl_texture()) {
    DCHECK(!memory_object);
    api->glTexStorage2DEXTFn(GL_TEXTURE_2D, 1, internal_format, size().width(),
                             size().height());
  } else {
    DCHECK(memory_object);
    api->glTexStorageMem2DEXTFn(GL_TEXTURE_2D, 1, internal_format,
                                size().width(), size().height(),
                                memory_object->id(), 0);
  }

  if (is_bgra8 && internal_format == GL_RGBA8) {
    api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_B, GL_RED);
    api->glTexParameteriFn(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
  }

  return texture_service_id;
}

std::unique_ptr<SharedImageRepresentationGLTexture>
ExternalVkImageBacking::ProduceGLTexture(SharedImageManager* manager,
                                         MemoryTypeTracker* tracker) {
  DCHECK(!texture_passthrough_);
  if (!(usage() & SHARED_IMAGE_USAGE_GLES2)) {
    DLOG(ERROR) << "The backing is not created with GLES2 usage.";
    return nullptr;
  }

  if (!texture_) {
    GLuint texture_service_id = ProduceGLTextureInternal();
    if (!texture_service_id)
      return nullptr;
    GLuint internal_format = viz::TextureStorageFormat(format());
    GLenum gl_format = viz::GLDataFormat(format());
    GLenum gl_type = viz::GLDataType(format());

    texture_ = new gles2::Texture(texture_service_id);
    texture_->SetLightweightRef();
    texture_->SetTarget(GL_TEXTURE_2D, 1);
    texture_->sampler_state_.min_filter = GL_LINEAR;
    texture_->sampler_state_.mag_filter = GL_LINEAR;
    texture_->sampler_state_.wrap_t = GL_CLAMP_TO_EDGE;
    texture_->sampler_state_.wrap_s = GL_CLAMP_TO_EDGE;
    // If the backing is already cleared, no need to clear it again.
    gfx::Rect cleared_rect;
    if (IsCleared())
      cleared_rect = gfx::Rect(size());

    texture_->SetLevelInfo(GL_TEXTURE_2D, 0, internal_format, size().width(),
                           size().height(), 1, 0, gl_format, gl_type,
                           cleared_rect);
    texture_->SetImmutable(true, true);
  }
  return std::make_unique<ExternalVkImageGLRepresentation>(
      manager, this, tracker, texture_, texture_->service_id());
}

std::unique_ptr<SharedImageRepresentationGLTexturePassthrough>
ExternalVkImageBacking::ProduceGLTexturePassthrough(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker) {
  DCHECK(!texture_);
  if (!(usage() & SHARED_IMAGE_USAGE_GLES2)) {
    DLOG(ERROR) << "The backing is not created with GLES2 usage.";
    return nullptr;
  }

  if (!texture_passthrough_) {
    GLuint texture_service_id = ProduceGLTextureInternal();
    if (!texture_service_id)
      return nullptr;
    GLuint internal_format = viz::TextureStorageFormat(format());
    GLenum gl_format = viz::GLDataFormat(format());
    GLenum gl_type = viz::GLDataType(format());

    texture_passthrough_ = base::MakeRefCounted<gpu::gles2::TexturePassthrough>(
        texture_service_id, GL_TEXTURE_2D, internal_format, size().width(),
        size().height(),
        /*depth=*/1, /*border=*/0, gl_format, gl_type);
  }

  return std::make_unique<ExternalVkImageGLPassthroughRepresentation>(
      manager, this, tracker, texture_passthrough_->service_id());
}

std::unique_ptr<SharedImageRepresentationSkia>
ExternalVkImageBacking::ProduceSkia(
    SharedImageManager* manager,
    MemoryTypeTracker* tracker,
    scoped_refptr<SharedContextState> context_state) {
  // This backing type is only used when vulkan is enabled, so SkiaRenderer
  // should also be using Vulkan.
  DCHECK_EQ(context_state_, context_state.get());
  DCHECK(context_state->GrContextIsVulkan());
  return std::make_unique<ExternalVkImageSkiaRepresentation>(manager, this,
                                                             tracker);
}

void ExternalVkImageBacking::InstallSharedMemory(
    base::WritableSharedMemoryMapping shared_memory_mapping,
    size_t stride,
    size_t memory_offset) {
  DCHECK(!shared_memory_mapping_.IsValid());
  DCHECK(shared_memory_mapping.IsValid());
  shared_memory_mapping_ = std::move(shared_memory_mapping);
  stride_ = stride;
  memory_offset_ = memory_offset;
  Update(nullptr);
}

void ExternalVkImageBacking::UpdateContent(uint32_t content_flags) {
  // Only support one backing for now.
  DCHECK(content_flags == kInVkImage || content_flags == kInGLTexture ||
         content_flags == kInSharedMemory);

  if ((latest_content_ & content_flags) == content_flags)
    return;

  if (content_flags == kInGLTexture && !use_separate_gl_texture())
    content_flags = kInVkImage;

  if (content_flags == kInVkImage) {
    if (latest_content_ & kInSharedMemory) {
      if (!shared_memory_mapping_.IsValid())
        return;
      auto pixel_data =
          shared_memory_mapping_.GetMemoryAsSpan<const uint8_t>().subspan(
              memory_offset_);
      if (!WritePixels(
              pixel_data.size(), stride_,
              base::BindOnce([](const void* data, size_t size,
                                void* buffer) { memcpy(buffer, data, size); },
                             pixel_data.data(), pixel_data.size()))) {
        return;
      }
      latest_content_ |=
          use_separate_gl_texture() ? kInVkImage : kInVkImage | kInGLTexture;
      return;
    }
    if ((latest_content_ & kInGLTexture) && use_separate_gl_texture()) {
      CopyPixelsFromGLTextureToVkImage();
      latest_content_ |= kInVkImage;
      return;
    }
  } else if (content_flags == kInGLTexture) {
    DCHECK(use_separate_gl_texture());
    if (latest_content_ & kInSharedMemory) {
      CopyPixelsFromShmToGLTexture();
    } else if (latest_content_ & kInVkImage) {
      NOTIMPLEMENTED_LOG_ONCE();
    }
  } else if (content_flags == kInSharedMemory) {
    // TODO(penghuang): read pixels back from VkImage to shared memory GMB, if
    // this feature is needed.
    NOTIMPLEMENTED_LOG_ONCE();
  }
}

bool ExternalVkImageBacking::WritePixels(size_t data_size,
                                         size_t stride,
                                         FillBufferCallback callback) {
  DCHECK(stride == 0 || size().height() * stride <= data_size);

  VkBufferCreateInfo buffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = data_size,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  VmaAllocator allocator =
      context_state()->vk_context_provider()->GetDeviceQueue()->vma_allocator();
  VkBuffer stage_buffer = VK_NULL_HANDLE;
  VmaAllocation stage_allocation = VK_NULL_HANDLE;
  VkResult result = vma::CreateBuffer(allocator, &buffer_create_info,
                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                      0, &stage_buffer, &stage_allocation);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vkCreateBuffer() failed." << result;
    return false;
  }

  void* buffer = nullptr;
  result = vma::MapMemory(allocator, stage_allocation, &buffer);
  if (result != VK_SUCCESS) {
    DLOG(ERROR) << "vma::MapMemory() failed. " << result;
    vma::DestroyBuffer(allocator, stage_buffer, stage_allocation);
    return false;
  }

  std::move(callback).Run(buffer);
  vma::UnmapMemory(allocator, stage_allocation);

  std::vector<gpu::SemaphoreHandle> handles;
  if (!BeginAccessInternal(false /* readonly */, &handles)) {
    DLOG(ERROR) << "BeginAccess() failed.";
    vma::DestroyBuffer(allocator, stage_buffer, stage_allocation);
    return false;
  }

  auto command_buffer = command_pool_->CreatePrimaryCommandBuffer();
  CHECK(command_buffer);
  {
    ScopedSingleUseCommandBufferRecorder recorder(*command_buffer);
    GrVkImageInfo image_info;
    bool success = backend_texture_.getVkImageInfo(&image_info);
    DCHECK(success);
    if (image_info.fImageLayout != VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      command_buffer->TransitionImageLayout(
          image_info.fImage, image_info.fImageLayout,
          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
      backend_texture_.setVkImageLayout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    }
    uint32_t buffer_width =
        stride ? stride * 8 / BitsPerPixel(format()) : size().width();
    command_buffer->CopyBufferToImage(stage_buffer, image_info.fImage,
                                      buffer_width, size().height(),
                                      size().width(), size().height());
  }

  if (!need_synchronization()) {
    DCHECK(handles.empty());
    command_buffer->Submit(0, nullptr, 0, nullptr);
    EndAccessInternal(false /* readonly */, SemaphoreHandle());

    auto* fence_helper = context_state_->vk_context_provider()
                             ->GetDeviceQueue()
                             ->GetFenceHelper();
    fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
        std::move(command_buffer));
    fence_helper->EnqueueBufferCleanupForSubmittedWork(stage_buffer,
                                                       stage_allocation);

    return true;
  }

  std::vector<VkSemaphore> begin_access_semaphores;
  begin_access_semaphores.reserve(handles.size() + 1);
  for (auto& handle : handles) {
    VkSemaphore semaphore = vulkan_implementation()->ImportSemaphoreHandle(
        device(), std::move(handle));
    begin_access_semaphores.emplace_back(semaphore);
  }

  VkSemaphore end_access_semaphore =
      vulkan_implementation()->CreateExternalSemaphore(device());
  command_buffer->Submit(begin_access_semaphores.size(),
                         begin_access_semaphores.data(), 1,
                         &end_access_semaphore);

  auto end_access_semphore_handle = vulkan_implementation()->GetSemaphoreHandle(
      device(), end_access_semaphore);
  EndAccessInternal(false /* readonly */,
                    std::move(end_access_semphore_handle));

  auto* fence_helper =
      context_state_->vk_context_provider()->GetDeviceQueue()->GetFenceHelper();
  fence_helper->EnqueueVulkanObjectCleanupForSubmittedWork(
      std::move(command_buffer));
  begin_access_semaphores.emplace_back(end_access_semaphore);
  fence_helper->EnqueueSemaphoresCleanupForSubmittedWork(
      begin_access_semaphores);
  fence_helper->EnqueueBufferCleanupForSubmittedWork(stage_buffer,
                                                     stage_allocation);

  return true;
}

void ExternalVkImageBacking::CopyPixelsFromGLTextureToVkImage() {
  DCHECK(use_separate_gl_texture());
  DCHECK_NE(!!texture_, !!texture_passthrough_);
  const GLuint texture_service_id =
      texture_ ? texture_->service_id() : texture_passthrough_->service_id();

  DCHECK_GE(format(), 0);
  DCHECK_LE(format(), viz::RESOURCE_FORMAT_MAX);
  auto gl_format = kFormatTable[format()].gl_format;
  auto gl_type = kFormatTable[format()].gl_type;
  auto bytes_per_pixel = kFormatTable[format()].bytes_per_pixel;

  if (gl_format == GL_ZERO) {
    NOTREACHED() << "Not supported resource format=" << format();
    return;
  }

  // Make sure GrContext is not using GL. So we don't need reset GrContext
  DCHECK(!context_state_->GrContextIsGL());

  // Make sure a gl context is current, since textures are shared between all gl
  // contexts, we don't care which gl context is current.
  if (!gl::GLContext::GetCurrent() &&
      !context_state_->MakeCurrent(nullptr, true /* needs_gl */))
    return;

  gl::GLApi* api = gl::g_current_gl_context;
  GLuint framebuffer;
  GLint old_framebuffer;
  api->glGetIntegervFn(GL_READ_FRAMEBUFFER_BINDING, &old_framebuffer);
  api->glGenFramebuffersEXTFn(1, &framebuffer);
  api->glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER, framebuffer);
  api->glFramebufferTexture2DEXTFn(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                   GL_TEXTURE_2D, texture_service_id, 0);
  GLenum status = api->glCheckFramebufferStatusEXTFn(GL_READ_FRAMEBUFFER);
  DCHECK_EQ(status, static_cast<GLenum>(GL_FRAMEBUFFER_COMPLETE))
      << "CheckFramebufferStatusEXT() failed.";

  base::CheckedNumeric<size_t> checked_size = bytes_per_pixel;
  checked_size *= size().width();
  checked_size *= size().height();
  DCHECK(checked_size.IsValid());

  ScopedPixelStore pack_row_length(api, GL_PACK_ROW_LENGTH, 0);
  ScopedPixelStore pack_skip_pixels(api, GL_PACK_SKIP_PIXELS, 0);
  ScopedPixelStore pack_skip_rows(api, GL_PACK_SKIP_ROWS, 0);
  ScopedPixelStore pack_aligment(api, GL_PACK_ALIGNMENT, 1);

  WritePixels(checked_size.ValueOrDie(), 0,
              base::BindOnce(
                  [](gl::GLApi* api, const gfx::Size& size, GLenum format,
                     GLenum type, void* buffer) {
                    api->glReadPixelsFn(0, 0, size.width(), size.height(),
                                        format, type, buffer);
                    DCHECK_EQ(api->glGetErrorFn(),
                              static_cast<GLenum>(GL_NO_ERROR));
                  },
                  api, size(), gl_format, gl_type));
  api->glBindFramebufferEXTFn(GL_READ_FRAMEBUFFER, old_framebuffer);
  api->glDeleteFramebuffersEXTFn(1, &framebuffer);
}

void ExternalVkImageBacking::CopyPixelsFromShmToGLTexture() {
  DCHECK(use_separate_gl_texture());
  DCHECK_NE(!!texture_, !!texture_passthrough_);
  const GLuint texture_service_id =
      texture_ ? texture_->service_id() : texture_passthrough_->service_id();

  DCHECK_GE(format(), 0);
  DCHECK_LE(format(), viz::RESOURCE_FORMAT_MAX);
  auto gl_format = kFormatTable[format()].gl_format;
  auto gl_type = kFormatTable[format()].gl_type;
  auto bytes_per_pixel = kFormatTable[format()].bytes_per_pixel;

  if (gl_format == GL_ZERO) {
    NOTREACHED() << "Not supported resource format=" << format();
    return;
  }

  // Make sure GrContext is not using GL. So we don't need reset GrContext
  DCHECK(!context_state_->GrContextIsGL());

  // Make sure a gl context is current, since textures are shared between all gl
  // contexts, we don't care which gl context is current.
  if (!gl::GLContext::GetCurrent() &&
      !context_state_->MakeCurrent(nullptr, true /* needs_gl */))
    return;

  gl::GLApi* api = gl::g_current_gl_context;
  GLint old_texture;
  api->glGetIntegervFn(GL_TEXTURE_BINDING_2D, &old_texture);
  api->glBindTextureFn(GL_TEXTURE_2D, texture_service_id);

  base::CheckedNumeric<size_t> checked_size = bytes_per_pixel;
  checked_size *= size().width();
  checked_size *= size().height();
  DCHECK(checked_size.IsValid());

  auto pixel_data =
      shared_memory_mapping_.GetMemoryAsSpan<const uint8_t>().subspan(
          memory_offset_);
  api->glTexSubImage2DFn(GL_TEXTURE_2D, 0, 0, 0, size().width(),
                         size().height(), gl_format, gl_type,
                         pixel_data.data());
  DCHECK_EQ(api->glGetErrorFn(), static_cast<GLenum>(GL_NO_ERROR));
  api->glBindTextureFn(GL_TEXTURE_2D, old_texture);
}

bool ExternalVkImageBacking::BeginAccessInternal(
    bool readonly,
    std::vector<SemaphoreHandle>* semaphore_handles) {
  DCHECK(semaphore_handles);
  DCHECK(semaphore_handles->empty());
  if (is_write_in_progress_) {
    DLOG(ERROR) << "Unable to begin read or write access because another write "
                   "access is in progress";
    return false;
  }

  if (reads_in_progress_ && !readonly) {
    DLOG(ERROR)
        << "Unable to begin write access because a read access is in progress";
    return false;
  }

  if (readonly) {
    DLOG_IF(ERROR, reads_in_progress_)
        << "Concurrent reading may cause problem.";
    ++reads_in_progress_;
    // If a shared image is read repeatedly without any write access,
    // |read_semaphore_handles_| will never be consumed and released, and then
    // chrome will run out of file descriptors. To avoid this problem, we wait
    // on read semaphores for readonly access too. And in most cases, a shared
    // image is only read from one vulkan device queue, so it should not have
    // performance impact.
    // TODO(penghuang): avoid waiting on read semaphores.
    *semaphore_handles = std::move(read_semaphore_handles_);
    read_semaphore_handles_.clear();

    // A semaphore will become unsignaled, when it has been signaled and waited,
    // so it is not safe to reuse it.
    if (write_semaphore_handle_.is_valid())
      semaphore_handles->push_back(std::move(write_semaphore_handle_));
  } else {
    is_write_in_progress_ = true;
    *semaphore_handles = std::move(read_semaphore_handles_);
    read_semaphore_handles_.clear();
    if (write_semaphore_handle_.is_valid())
      semaphore_handles->push_back(std::move(write_semaphore_handle_));
  }
  return true;
}

void ExternalVkImageBacking::EndAccessInternal(
    bool readonly,
    SemaphoreHandle semaphore_handle) {
  if (readonly) {
    DCHECK_GT(reads_in_progress_, 0u);
    --reads_in_progress_;
  } else {
    DCHECK(is_write_in_progress_);
    is_write_in_progress_ = false;
  }

  if (need_synchronization()) {
    DCHECK(semaphore_handle.is_valid());
    if (readonly) {
      read_semaphore_handles_.push_back(std::move(semaphore_handle));
    } else {
      DCHECK(!write_semaphore_handle_.is_valid());
      DCHECK(read_semaphore_handles_.empty());
      write_semaphore_handle_ = std::move(semaphore_handle);
    }
  } else {
    DCHECK(!semaphore_handle.is_valid());
  }
}

}  // namespace gpu
