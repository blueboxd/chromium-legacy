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
    case gfx::BufferFormat::YVU_420:
      return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;

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
    : SysmemBufferCollection(gfx::SysmemBufferCollectionId::Create()) {}

SysmemBufferCollection::SysmemBufferCollection(gfx::SysmemBufferCollectionId id)
    : id_(id) {}

bool SysmemBufferCollection::Initialize(
    fuchsia::sysmem::Allocator_Sync* allocator,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    VkDevice vk_device,
    size_t num_buffers) {
  DCHECK(IsNativePixmapConfigSupported(format, usage));
  DCHECK(!collection_);
  DCHECK(!vk_buffer_collection_);

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

  // sysmem currently doesn't support R8 and R8G8 images. To workaround this
  // issue they are masqueraded as RGBA images with shorter rows.
  size_t width_divider = 1;
  if (format_ == gfx::BufferFormat::R_8) {
    width_divider = 4;
  } else if (format_ == gfx::BufferFormat::RG_88) {
    width_divider = 2;
  }

  fuchsia::sysmem::ImageFormatConstraints image_format_constraints;
  image_format_constraints.pixel_format =
      BufferFormatToSysmemPixelFormat(format);
  image_format_constraints.min_coded_width = size_.width() / width_divider;
  image_format_constraints.max_coded_width =
      std::numeric_limits<uint32_t>::max();
  image_format_constraints.min_coded_height = size_.height();
  image_format_constraints.max_coded_height =
      std::numeric_limits<uint32_t>::max();
  image_format_constraints.min_bytes_per_row =
      gfx::RowSizeForBufferFormat(size_.width(), format, 0);
  image_format_constraints.max_bytes_per_row =
      std::numeric_limits<uint32_t>::max();
  image_format_constraints.color_spaces_count = 1;

  if (format == gfx::BufferFormat::YUV_420_BIPLANAR) {
    image_format_constraints.color_space[0].type =
        fuchsia::sysmem::ColorSpaceType::REC709;
  } else {
    image_format_constraints.color_space[0].type =
        fuchsia::sysmem::ColorSpaceType::SRGB;
  }

  return InitializeInternal(
      allocator, std::move(collection_token), num_buffers,
      base::make_optional(std::move(image_format_constraints)));
}

bool SysmemBufferCollection::Initialize(
    fuchsia::sysmem::Allocator_Sync* allocator,
    VkDevice vk_device,
    zx::channel token_handle) {
  DCHECK(!collection_);
  DCHECK(!vk_buffer_collection_);

  usage_ = gfx::BufferUsage::GPU_READ;
  vk_device_ = vk_device;

  // Assume that all imported collections are in NV12 format.
  format_ = gfx::BufferFormat::YUV_420_BIPLANAR;

  fuchsia::sysmem::BufferCollectionTokenSyncPtr token;
  token.Bind(std::move(token_handle));

  return InitializeInternal(allocator, std::move(token),
                            /*buffers_for_camping=*/0, base::nullopt);
}

scoped_refptr<gfx::NativePixmap> SysmemBufferCollection::CreateNativePixmap(
    size_t buffer_index) {
  CHECK_LT(buffer_index, num_buffers());

  gfx::NativePixmapHandle handle;
  handle.buffer_collection_id = id();
  handle.buffer_index = buffer_index;
  handle.ram_coherency =
      buffers_info_.settings.buffer_settings.coherency_domain ==
      fuchsia::sysmem::CoherencyDomain::RAM;

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

bool SysmemBufferCollection::CreateVkImage(
    size_t buffer_index,
    VkDevice vk_device,
    gfx::Size size,
    VkImage* vk_image,
    VkImageCreateInfo* vk_image_info,
    VkDeviceMemory* vk_device_memory,
    VkDeviceSize* mem_allocation_size,
    base::Optional<gpu::VulkanYCbCrInfo>* ycbcr_info) {
  DCHECK_CALLED_ON_VALID_THREAD(vulkan_thread_checker_);

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

  InitializeImageCreateInfo(vk_image_info, size);

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
  alloc_info.allocationSize = requirements.size;
  alloc_info.memoryTypeIndex = memory_type;

  if (vkAllocateMemory(vk_device_, &alloc_info, nullptr, vk_device_memory) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create VkMemory from sysmem buffer.";
    vkDestroyImage(vk_device_, *vk_image, nullptr);
    *vk_image = VK_NULL_HANDLE;
    return false;
  }

  if (vkBindImageMemory(vk_device_, *vk_image, *vk_device_memory, 0u) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to bind sysmem buffer to a VkImage.";
    vkDestroyImage(vk_device_, *vk_image, nullptr);
    *vk_image = VK_NULL_HANDLE;
    vkFreeMemory(vk_device_, *vk_device_memory, nullptr);
    *vk_device_memory = VK_NULL_HANDLE;
    return false;
  }

  *mem_allocation_size = requirements.size;

  auto color_space =
      buffers_info_.settings.image_format_constraints.color_space[0].type;
  switch (color_space) {
    case fuchsia::sysmem::ColorSpaceType::SRGB:
      *ycbcr_info = base::nullopt;
      break;

    case fuchsia::sysmem::ColorSpaceType::REC709: {
      VkFormatFeatureFlags format_features =
          VK_FORMAT_FEATURE_COSITED_CHROMA_SAMPLES_BIT |
          VK_FORMAT_FEATURE_MIDPOINT_CHROMA_SAMPLES_BIT |
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_YCBCR_CONVERSION_LINEAR_FILTER_BIT;

      // Currently sysmem doesn't specify location of chroma samples relative to
      // luma (see MTWN-397). Assume they are cosited with luma.
      *ycbcr_info = gpu::VulkanYCbCrInfo(
          vk_image_info->format, /*external_format=*/0,
          VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709,
          VK_SAMPLER_YCBCR_RANGE_ITU_NARROW, VK_CHROMA_LOCATION_COSITED_EVEN,
          VK_CHROMA_LOCATION_COSITED_EVEN, format_features);
      break;
    }

    default:
      DLOG(ERROR) << "Sysmem allocated buffer with unsupported color space: "
                  << static_cast<int>(color_space);
      return false;
  }

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

bool SysmemBufferCollection::InitializeInternal(
    fuchsia::sysmem::Allocator_Sync* allocator,
    fuchsia::sysmem::BufferCollectionTokenSyncPtr collection_token,
    size_t buffers_for_camping,
    base::Optional<fuchsia::sysmem::ImageFormatConstraints>
        image_format_constraints) {
  fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
      collection_token_for_vulkan;
  collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                              collection_token_for_vulkan.NewRequest());
  zx_status_t status = collection_token->Sync();
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

  fuchsia::sysmem::BufferCollectionConstraints constraints;
  if (is_mappable()) {
    constraints.usage.cpu =
        fuchsia::sysmem::cpuUsageRead | fuchsia::sysmem::cpuUsageWrite;

    constraints.has_buffer_memory_constraints = true;
    constraints.buffer_memory_constraints.ram_domain_supported = true;
    constraints.buffer_memory_constraints.cpu_domain_supported = true;
  } else {
    constraints.usage.none = fuchsia::sysmem::noneUsage;
  }

  constraints.min_buffer_count_for_camping = buffers_for_camping;

  if (image_format_constraints) {
    constraints.image_format_constraints_count = 1;
    constraints.image_format_constraints[0] = image_format_constraints.value();
  } else {
    constraints.image_format_constraints_count = 0;
  }

  status = collection_->SetConstraints(/*has_constraints=*/true,
                                       std::move(constraints));
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status)
        << "fuchsia.sysmem.BufferCollection.SetConstraints()";
    return false;
  }

  zx::channel token_channel = collection_token_for_vulkan.TakeChannel();
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
  InitializeImageCreateInfo(&image_create_info, size_);

  // sysmem currently doesn't support R8 and R8G8 images. To workaround this
  // issue they are masqueraded as RGBA images with shorter rows.
  if (format_ == gfx::BufferFormat::R_8) {
    image_create_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    image_create_info.extent.width = size_.width() / 4;
  } else if (format_ == gfx::BufferFormat::RG_88) {
    image_create_info.format = VK_FORMAT_B8G8R8A8_UNORM;
    image_create_info.extent.width = size_.width() / 2;
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

  DCHECK_GE(buffers_info_.buffer_count, buffers_for_camping);
  DCHECK(buffers_info_.settings.has_image_format_constraints);

  buffer_size_ = buffers_info_.settings.buffer_settings.size_bytes;

  // CreateVkImage() should always be called on the same thread, but it may be
  // different from the thread that called Initialize().
  DETACH_FROM_THREAD(vulkan_thread_checker_);

  return true;
}

void SysmemBufferCollection::InitializeImageCreateInfo(
    VkImageCreateInfo* vk_image_info,
    gfx::Size size) {
  *vk_image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  vk_image_info->imageType = VK_IMAGE_TYPE_2D;
  vk_image_info->format = VkFormatForBufferFormat(format_);
  vk_image_info->extent = VkExtent3D{size.width(), size.height(), 1};
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
