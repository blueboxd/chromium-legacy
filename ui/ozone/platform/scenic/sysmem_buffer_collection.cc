// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"

#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/native_pixmap.h"

namespace ui {

namespace {

class SysmemNativePixmap : public gfx::NativePixmap {
 public:
  SysmemNativePixmap(scoped_refptr<SysmemBufferCollection> collection,
                     gfx::NativePixmapHandle handle)
      : collection_(collection), handle_(std::move(handle)) {}

  bool AreDmaBufFdsValid() const override { return false; }
  int GetDmaBufFd(size_t plane) const override {
    NOTREACHED();
    return -1;
  }
  uint32_t GetDmaBufPitch(size_t plane) const override {
    NOTREACHED();
    return 0u;
  }
  size_t GetDmaBufOffset(size_t plane) const override {
    NOTREACHED();
    return 0u;
  }
  size_t GetDmaBufPlaneSize(size_t plane) const override {
    NOTREACHED();
    return 0;
  }
  size_t GetNumberOfPlanes() const override {
    NOTREACHED();
    return 0;
  }
  uint64_t GetBufferFormatModifier() const override {
    NOTREACHED();
    return 0;
  }

  gfx::BufferFormat GetBufferFormat() const override {
    return collection_->format();
  }
  gfx::Size GetBufferSize() const override { return collection_->size(); }
  uint32_t GetUniqueId() const override { return 0; }
  bool ScheduleOverlayPlane(gfx::AcceleratedWidget widget,
                            int plane_z_order,
                            gfx::OverlayTransform plane_transform,
                            const gfx::Rect& display_bounds,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override {
    NOTIMPLEMENTED();

    return false;
  }
  gfx::NativePixmapHandle ExportHandle() override {
    return gfx::CloneHandleForIPC(handle_);
  }

 private:
  ~SysmemNativePixmap() override = default;

  // Keep reference to the collection to make sure it outlives the pixmap.
  scoped_refptr<SysmemBufferCollection> collection_;
  gfx::NativePixmapHandle handle_;

  DISALLOW_COPY_AND_ASSIGN(SysmemNativePixmap);
};

size_t RoundUp(size_t value, size_t alignment) {
  return ((value + alignment - 1) / alignment) * alignment;
}

VkFormat VkFormatForBufferFormat(gfx::BufferFormat buffer_format) {
  switch (buffer_format) {
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;

    case gfx::BufferFormat::R_8:
      return VK_FORMAT_R8_UNORM;

    case gfx::BufferFormat::RG_88:
      return VK_FORMAT_R8G8_UNORM;

    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      return VK_FORMAT_B8G8R8A8_UNORM;

    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return VK_FORMAT_R8G8B8A8_UNORM;

    default:
      NOTREACHED();
      return VK_FORMAT_UNDEFINED;
  }
}

fuchsia::sysmem::PixelFormat BufferFormatToSysmemPixelFormat(
    gfx::BufferFormat format) {
  fuchsia::sysmem::PixelFormat result;
  switch (format) {
    case gfx::BufferFormat::YUV_420_BIPLANAR:
      result.type = fuchsia::sysmem::PixelFormatType::NV12;
      break;

    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RG_88:
      // sysmem currently doesn't support R8 and R8G8 images. To workaround this
      // issue they are masqueraded as RGBA images with shorter rows.
      result.type = fuchsia::sysmem::PixelFormatType::BGRA32;
      break;

    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      result.type = fuchsia::sysmem::PixelFormatType::BGRA32;
      break;

    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      result.type = fuchsia::sysmem::PixelFormatType::R8G8B8A8;
      break;

    default:
      NOTREACHED();
  }
  return result;
}

}  // namespace

// static
bool SysmemBufferCollection::IsNativePixmapConfigSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  bool format_supported = format == gfx::BufferFormat::YUV_420_BIPLANAR ||
                          format == gfx::BufferFormat::R_8 ||
                          format == gfx::BufferFormat::RG_88 ||
                          format == gfx::BufferFormat::RGBA_8888 ||
                          format == gfx::BufferFormat::RGBX_8888 ||
                          format == gfx::BufferFormat::BGRA_8888 ||
                          format == gfx::BufferFormat::BGRX_8888;
  bool usage_supported = usage == gfx::BufferUsage::SCANOUT ||
                         usage == gfx::BufferUsage::SCANOUT_CPU_READ_WRITE ||
                         usage == gfx::BufferUsage::GPU_READ_CPU_READ_WRITE;
  return format_supported && usage_supported;
}

SysmemBufferCollection::SysmemBufferCollection()
    : id_(gfx::SysmemBufferCollectionId::Create()) {}

bool SysmemBufferCollection::Initialize(
    fuchsia::sysmem::Allocator_Sync* allocator,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    VkDevice vk_device,
    size_t num_buffers) {
  DCHECK(IsNativePixmapConfigSupported(format, usage));
  DCHECK(!collection_);

  // Currently all supported |usage| values require GPU access, which requires
  // a valid VkDevice.
  if (vk_device == VK_NULL_HANDLE)
    return false;

  size_ = size;
  format_ = format;
  usage_ = usage;
  vk_device_ = vk_device;

  fuchsia::sysmem::BufferCollectionTokenSyncPtr collection_token;
  zx_status_t status =
      allocator->AllocateSharedCollection(collection_token.NewRequest());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status)
        << "fuchsia.sysmem.Allocator.AllocateSharedCollection()";
    return false;
  }

  fuchsia::sysmem::BufferCollectionTokenPtr vulkan_collection_token;
  collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                              vulkan_collection_token.NewRequest());
  status = collection_token->Sync();
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollectionToken.Sync()";
    return false;
  }

  status = allocator->BindSharedCollection(std::move(collection_token),
                                           collection_.NewRequest());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.Allocator.BindSharedCollection()";
    return false;
  }

  // sysmem currently doesn't support R8 and R8G8 images. To workaround this
  // issue they are masqueraded as RGBA images with shorter rows.
  size_t width_divider = 1;
  if (format_ == gfx::BufferFormat::R_8) {
    width_divider = 4;
  } else if (format_ == gfx::BufferFormat::RG_88) {
    width_divider = 2;
  }

  fuchsia::sysmem::BufferCollectionConstraints constraints;
  if (is_mappable()) {
    constraints.usage.cpu =
        fuchsia::sysmem::cpuUsageRead | fuchsia::sysmem::cpuUsageWrite;
  }
  constraints.min_buffer_count_for_camping = num_buffers;
  constraints.image_format_constraints_count = 0;

  constraints.image_format_constraints_count = 1;
  constraints.image_format_constraints[0].pixel_format =
      BufferFormatToSysmemPixelFormat(format);
  constraints.image_format_constraints[0].min_coded_width =
      size_.width() / width_divider;
  constraints.image_format_constraints[0].max_coded_width =
      std::numeric_limits<uint32_t>::max();
  constraints.image_format_constraints[0].min_coded_height = size_.height();
  constraints.image_format_constraints[0].max_coded_height =
      std::numeric_limits<uint32_t>::max();
  constraints.image_format_constraints[0].min_bytes_per_row =
      gfx::RowSizeForBufferFormat(size_.width(), format, 0);
  constraints.image_format_constraints[0].max_bytes_per_row =
      std::numeric_limits<uint32_t>::max();
  constraints.image_format_constraints[0].color_spaces_count = 1;

  if (format == gfx::BufferFormat::YUV_420_BIPLANAR) {
    constraints.image_format_constraints[0].color_space[0].type =
        fuchsia::sysmem::ColorSpaceType::REC709;
  } else {
    constraints.image_format_constraints[0].color_space[0].type =
        fuchsia::sysmem::ColorSpaceType::SRGB;
  }

  status = collection_->SetConstraints(/*has_constraints=*/true,
                                       std::move(constraints));
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status)
        << "fuchsia.sysmem.BufferCollection.SetConstraints()";
    return false;
  }

  zx::channel token_channel = vulkan_collection_token.Unbind().TakeChannel();
  VkBufferCollectionCreateInfoFUCHSIA buffer_collection_create_info = {
      VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CREATE_INFO_FUCHSIA};
  buffer_collection_create_info.collectionToken = token_channel.get();
  if (vkCreateBufferCollectionFUCHSIA(vk_device_,
                                      &buffer_collection_create_info, nullptr,
                                      &vk_buffer_collection_) != VK_SUCCESS) {
    vk_buffer_collection_ = VK_NULL_HANDLE;
    DLOG(ERROR) << "vkCreateBufferCollectionFUCHSIA() failed";
    return false;
  }

  // vkCreateBufferCollectionFUCHSIA() takes ownership of the token on success.
  ignore_result(token_channel.release());

  VkImageCreateInfo image_create_info;
  InitializeImageCreateInfo(&image_create_info);

  if (width_divider > 1) {
    image_create_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    image_create_info.extent.width = size_.width() / width_divider;
  }

  if (vkSetBufferCollectionConstraintsFUCHSIA(vk_device_, vk_buffer_collection_,
                                              &image_create_info) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "vkSetBufferCollectionConstraintsFUCHSIA() failed";
    return false;
  }

  zx_status_t wait_status;
  status = collection_->WaitForBuffersAllocated(&wait_status, &buffers_info_);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollection failed";
    return false;
  }

  if (wait_status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollection::"
                              "WaitForBuffersAllocated() failed.";
    return false;
  }

  DCHECK_GE(buffers_info_.buffer_count, num_buffers);
  DCHECK(buffers_info_.settings.has_image_format_constraints);

  buffer_size_ = buffers_info_.settings.buffer_settings.size_bytes;

  // CreateVkImage() should always be called on the same thread, but it may be
  // different from the thread that called Initialize().
  DETACH_FROM_THREAD(vulkan_thread_checker_);

  return true;
}

scoped_refptr<gfx::NativePixmap> SysmemBufferCollection::CreateNativePixmap(
    size_t buffer_index) {
  CHECK_LT(buffer_index, num_buffers());

  gfx::NativePixmapHandle handle;
  handle.buffer_collection_id = id();
  handle.buffer_index = buffer_index;

  zx::vmo main_plane_vmo;
  if (is_mappable()) {
    DCHECK(buffers_info_.buffers[buffer_index].vmo.is_valid());
    zx_status_t status = buffers_info_.buffers[buffer_index].vmo.duplicate(
        ZX_RIGHT_SAME_RIGHTS, &main_plane_vmo);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
      return nullptr;
    }
  }

  const fuchsia::sysmem::ImageFormatConstraints& format =
      buffers_info_.settings.image_format_constraints;

  size_t stride = RoundUp(format.min_bytes_per_row, format.coded_width_divisor);
  size_t plane_offset = buffers_info_.buffers[buffer_index].vmo_usable_start;
  size_t plane_size = stride * format.min_coded_height;
  handle.planes.emplace_back(stride, plane_offset, plane_size,
                             std::move(main_plane_vmo));

  // For YUV images add a second plane.
  if (format_ == gfx::BufferFormat::YUV_420_BIPLANAR) {
    size_t uv_plane_offset = plane_offset + plane_size;
    size_t uv_plane_size = plane_size / 2;
    handle.planes.emplace_back(stride, uv_plane_offset, uv_plane_size,
                               zx::vmo());
    DCHECK_LE(uv_plane_offset + uv_plane_size, buffer_size_);
  }

  return new SysmemNativePixmap(this, std::move(handle));
}

bool SysmemBufferCollection::CreateVkImage(size_t buffer_index,
                                           VkDevice vk_device,
                                           VkImage* vk_image,
                                           VkImageCreateInfo* vk_image_info,
                                           VkDeviceMemory* vk_device_memory,
                                           VkDeviceSize* mem_allocation_size) {
  DCHECK_CALLED_ON_VALID_THREAD(vulkan_thread_checker_);
  DCHECK_LT(buffer_index, num_buffers());

  if (vk_device_ != vk_device) {
    DLOG(FATAL) << "Tried to import NativePixmap that was created for a "
                   "different VkDevice.";
    return false;
  }

  VkBufferCollectionPropertiesFUCHSIA properties = {
      VK_STRUCTURE_TYPE_BUFFER_COLLECTION_PROPERTIES_FUCHSIA};
  if (vkGetBufferCollectionPropertiesFUCHSIA(vk_device_, vk_buffer_collection_,
                                             &properties) != VK_SUCCESS) {
    DLOG(ERROR) << "vkGetBufferCollectionPropertiesFUCHSIA failed";
    return false;
  }

  InitializeImageCreateInfo(vk_image_info);
  if (vkCreateImage(vk_device_, vk_image_info, nullptr, vk_image) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create VkImage.";
    return false;
  }

  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(vk_device, *vk_image, &requirements);

  uint32_t viable_memory_types =
      properties.memoryTypeBits & requirements.memoryTypeBits;
  uint32_t memory_type = base::bits::CountTrailingZeroBits(viable_memory_types);

  VkImportMemoryBufferCollectionFUCHSIA buffer_collection_info = {
      VK_STRUCTURE_TYPE_IMPORT_MEMORY_BUFFER_COLLECTION_FUCHSIA};
  buffer_collection_info.collection = vk_buffer_collection_;
  buffer_collection_info.index = buffer_index;

  VkMemoryAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                     &buffer_collection_info};
  alloc_info.allocationSize = buffer_size_;
  alloc_info.memoryTypeIndex = memory_type;

  if (vkAllocateMemory(vk_device_, &alloc_info, nullptr, vk_device_memory) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create import sysmem buffer.";
    vkDestroyImage(vk_device_, *vk_image, nullptr);
    *vk_image = VK_NULL_HANDLE;
    return false;
  }

  if (vkBindImageMemory(vk_device_, *vk_image, *vk_device_memory, 0u) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to bind sysmem buffer to an image.";
    vkDestroyImage(vk_device_, *vk_image, nullptr);
    *vk_image = VK_NULL_HANDLE;
    vkFreeMemory(vk_device_, *vk_device_memory, nullptr);
    *vk_device_memory = VK_NULL_HANDLE;
    return false;
  }

  *mem_allocation_size = buffer_size_;

  return true;
}

void SysmemBufferCollection::SetOnDeletedCallback(
    base::OnceClosure on_deleted) {
  DCHECK(!on_deleted_);
  on_deleted_ = std::move(on_deleted);
}

SysmemBufferCollection::~SysmemBufferCollection() {
  if (vk_buffer_collection_ != VK_NULL_HANDLE) {
    vkDestroyBufferCollectionFUCHSIA(vk_device_, vk_buffer_collection_,
                                     nullptr);
  }

  if (collection_)
    collection_->Close();

  if (on_deleted_)
    std::move(on_deleted_).Run();
}

void SysmemBufferCollection::InitializeImageCreateInfo(
    VkImageCreateInfo* vk_image_info) {
  *vk_image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  vk_image_info->imageType = VK_IMAGE_TYPE_2D;
  vk_image_info->format = VkFormatForBufferFormat(format_);
  vk_image_info->extent = VkExtent3D{size_.width(), size_.height(), 1};
  vk_image_info->mipLevels = 1;
  vk_image_info->arrayLayers = 1;
  vk_image_info->samples = VK_SAMPLE_COUNT_1_BIT;
  vk_image_info->tiling =
      is_mappable() ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
  vk_image_info->usage = VK_IMAGE_USAGE_SAMPLED_BIT;
  vk_image_info->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vk_image_info->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

}  // namespace ui
