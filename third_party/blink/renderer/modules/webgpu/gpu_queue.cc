// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"

#include "build/build_config.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/client/webgpu_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_enforce_range_sequence_or_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_enforce_range_sequence_or_gpu_origin_2d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_enforce_range_sequence_or_gpu_origin_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_command_buffer_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_fence_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_image_bitmap_copy_view.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_copy_view.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/modules/webgpu/client_validation.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_fence.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_image_bitmap_handler.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

namespace {

WGPUOrigin3D GPUOrigin2DToWGPUOrigin3D(
    const UnsignedLongEnforceRangeSequenceOrGPUOrigin2DDict* webgpu_origin) {
  DCHECK(webgpu_origin);

  WGPUOrigin3D dawn_origin = {};

  if (webgpu_origin->IsUnsignedLongEnforceRangeSequence()) {
    const Vector<uint32_t>& webgpu_origin_sequence =
        webgpu_origin->GetAsUnsignedLongEnforceRangeSequence();
    DCHECK_EQ(webgpu_origin_sequence.size(), 3UL);
    dawn_origin.x = webgpu_origin_sequence[0];
    dawn_origin.y = webgpu_origin_sequence[1];
    dawn_origin.z = 0;
  } else if (webgpu_origin->IsGPUOrigin2DDict()) {
    const GPUOrigin2DDict* webgpu_origin_2d_dict =
        webgpu_origin->GetAsGPUOrigin2DDict();
    dawn_origin.x = webgpu_origin_2d_dict->x();
    dawn_origin.y = webgpu_origin_2d_dict->y();
    dawn_origin.z = 0;
  } else {
    NOTREACHED();
  }

  return dawn_origin;
}

bool AreCompatibleFormatForImageBitmapGPUCopy(
    SkColorType sk_color_type,
    WGPUTextureFormat dawn_texture_format) {
  switch (dawn_texture_format) {
    case WGPUTextureFormat_RGBA8Unorm:
      return sk_color_type == SkColorType::kRGBA_8888_SkColorType;
    case WGPUTextureFormat_BGRA8Unorm:
      return sk_color_type == SkColorType::kBGRA_8888_SkColorType;
    case WGPUTextureFormat_RGB10A2Unorm:
      return sk_color_type == SkColorType::kRGBA_1010102_SkColorType;
    case WGPUTextureFormat_RGBA16Float:
      return sk_color_type == SkColorType::kRGBA_F16_SkColorType;
    case WGPUTextureFormat_RGBA32Float:
      return sk_color_type == SkColorType::kRGBA_F32_SkColorType;
    case WGPUTextureFormat_RG8Unorm:
      return sk_color_type == SkColorType::kR8G8_unorm_SkColorType;
    case WGPUTextureFormat_RG16Float:
      return sk_color_type == SkColorType::kR16G16_float_SkColorType;
    default:
      return false;
  }
}

bool CanUploadThroughGPU(StaticBitmapImage* image,
                         const CanvasColorParams& color_param,
                         GPUTexture* dest_texture) {
  // Cannot handle top left origin image
  if (image->CurrentFrameOrientation().Orientation() !=
      ImageOrientationEnum::kOriginBottomLeft) {
    return false;
  }
  // Cannot handle source and dest texture have uncompatible format
  if (!AreCompatibleFormatForImageBitmapGPUCopy(color_param.GetSkColorType(),
                                                dest_texture->Format())) {
    return false;
  }

  // Only Windows platform can try this path now
  // TODO(shaobo.yan@intel.com) : release this condition for all passthrough
  // platform
#if defined(OS_WIN)
  // TODO(shaobo.yan@intel.com): Need to figure out color space and
  // pre/unmultiply alpha
  return true;
#else
  return false;
#endif  // defined(OS_WIN)
}
}  // anonymous namespace

GPUQueue::GPUQueue(GPUDevice* device, WGPUQueue queue)
    : DawnObject<WGPUQueue>(device, queue) {
  produce_dawn_texture_handler_ = base::AdoptRef(new DawnTextureFromImageBitmap(
      GetDawnControlClient(), device_->GetClientID()));
}

GPUQueue::~GPUQueue() {
  produce_dawn_texture_handler_ = nullptr;

  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().queueRelease(GetHandle());
}

void GPUQueue::submit(const HeapVector<Member<GPUCommandBuffer>>& buffers) {
  std::unique_ptr<WGPUCommandBuffer[]> commandBuffers = AsDawnType(buffers);

  GetProcs().queueSubmit(GetHandle(), buffers.size(), commandBuffers.get());
  // WebGPU guarantees that submitted commands finish in finite time so we
  // flush commands to the GPU process now.
  device_->GetInterface()->FlushCommands();
}

void GPUQueue::signal(GPUFence* fence, uint64_t signal_value) {
  GetProcs().queueSignal(GetHandle(), fence->GetHandle(), signal_value);
  // Signaling a fence adds a callback to update the fence value to the
  // completed value. WebGPU guarantees that the fence completion is
  // observable in finite time so we flush commands to the GPU process now.
  device_->GetInterface()->FlushCommands();
}

GPUFence* GPUQueue::createFence(const GPUFenceDescriptor* descriptor) {
  DCHECK(descriptor);

  WGPUFenceDescriptor desc = {};
  desc.nextInChain = nullptr;
  desc.initialValue = descriptor->initialValue();
  if (descriptor->hasLabel()) {
    desc.label = descriptor->label().Utf8().data();
  }

  return MakeGarbageCollected<GPUFence>(
      device_, GetProcs().queueCreateFence(GetHandle(), &desc));
}

// TODO(shaobo.yan@intel.com): Implement this function
void GPUQueue::copyImageBitmapToTexture(
    GPUImageBitmapCopyView* source,
    GPUTextureCopyView* destination,
    UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& copy_size,
    ExceptionState& exception_state) {
  if (!source->imageBitmap()) {
    exception_state.ThrowTypeError("No valid imageBitmap");
    return;
  }

  // TODO(shaobo.yan@intel.com): only the same color format texture copy allowed
  // now. Need to Explore compatible texture format copy.
  if (!ValidateCopySize(copy_size, exception_state) ||
      !ValidateTextureCopyView(destination, exception_state)) {
    return;
  }

  scoped_refptr<StaticBitmapImage> image = source->imageBitmap()->BitmapImage();


  // TODO(shaobo.yan@intel.com) : Check that the destination GPUTexture has an
  // appropriate format. Now only support texture format exactly the same. The
  // compatible formats need to be defined in WebGPU spec.

  WGPUExtent3D dawn_copy_size = AsDawnType(&copy_size);

  // Extract imageBitmap attributes
  WGPUOrigin3D origin_in_image_bitmap =
      GPUOrigin2DToWGPUOrigin3D(&(source->origin()));

  // Validate origin value
  if (static_cast<uint32_t>(image->width()) <= origin_in_image_bitmap.x ||
      static_cast<uint32_t>(image->height()) <= origin_in_image_bitmap.y) {
    exception_state.ThrowRangeError(
        "Copy origin is out of bounds of imageBitmap.");
    return;
  }

  // Validate the copy rect is inside the imageBitmap
  if (image->width() - origin_in_image_bitmap.x < dawn_copy_size.width ||
      image->height() - origin_in_image_bitmap.y < dawn_copy_size.height) {
    exception_state.ThrowRangeError(
        "Copy rect is out of bounds of imageBitmap.");
    return;
  }

  WGPUTextureCopyView dawn_destination = AsDawnType(destination);

  const CanvasColorParams& color_params =
      source->imageBitmap()->GetCanvasColorParams();

  // TODO(shaobo.yan@intel.com): Implement GPU copy path
  // Try GPU path first.
  if (image->IsTextureBacked()) {  // Try GPU uploading path.
    if (CanUploadThroughGPU(image.get(), color_params,
                            destination->texture())) {
      if (CopyContentFromGPU(image.get(), origin_in_image_bitmap,
                             dawn_copy_size, dawn_destination)) {
        return;
      }
    }
    // GPU path failed, fallback to CPU path
    image = image->MakeUnaccelerated();
  }
  // CPU path is the fallback path and should always work.
  if (!CopyContentFromCPU(image.get(), color_params, origin_in_image_bitmap,
                          dawn_copy_size, dawn_destination)) {
    exception_state.ThrowTypeError("Failed to copy content from imageBitmap.");
    return;
  }
}

bool GPUQueue::CopyContentFromCPU(StaticBitmapImage* image,
                                  const CanvasColorParams& color_params,
                                  const WGPUOrigin3D& origin,
                                  const WGPUExtent3D& copy_size,
                                  const WGPUTextureCopyView& destination) {
  // Prepare for uploading CPU data.
  IntRect image_data_rect(origin.x, origin.y, copy_size.width,
                          copy_size.height);
  WebGPUImageUploadSizeInfo info =
      ComputeImageBitmapWebGPUUploadSizeInfo(image_data_rect, color_params);

  // Create a mapped buffer to receive image bitmap contents
  WGPUBufferDescriptor buffer_desc;
  buffer_desc.nextInChain = nullptr;
  buffer_desc.label = nullptr;
  buffer_desc.usage = WGPUBufferUsage_CopySrc;
  buffer_desc.size = info.size_in_bytes;

  WGPUCreateBufferMappedResult result =
      GetProcs().deviceCreateBufferMapped(device_->GetHandle(), &buffer_desc);

  if (!CopyBytesFromImageBitmapForWebGPU(
          image,
          base::span<uint8_t>(reinterpret_cast<uint8_t*>(result.data),
                              static_cast<size_t>(result.dataLength)),
          image_data_rect, color_params)) {
    // Release the buffer.
    GetProcs().bufferRelease(result.buffer);
    return false;
  }

  GetProcs().bufferUnmap(result.buffer);

  // Start a B2T copy to move contents from buffer to destination texture
  WGPUBufferCopyView dawn_intermediate = {};
  dawn_intermediate.nextInChain = nullptr;
  dawn_intermediate.buffer = result.buffer;
  dawn_intermediate.offset = 0;
  dawn_intermediate.rowPitch = info.wgpu_row_pitch;
  dawn_intermediate.imageHeight = image->height();

  WGPUCommandEncoder encoder =
      GetProcs().deviceCreateCommandEncoder(device_->GetHandle(), nullptr);
  GetProcs().commandEncoderCopyBufferToTexture(encoder, &dawn_intermediate,
                                               &destination, &copy_size);
  WGPUCommandBuffer commands =
      GetProcs().commandEncoderFinish(encoder, nullptr);

  // Don't need to add fence after this submit. Because if user want to use the
  // texture to do copy or render, it will trigger another queue submit. Dawn
  // will insert the necessary resource transitions.
  GetProcs().queueSubmit(GetHandle(), 1, &commands);

  // Release intermediate resources.
  GetProcs().commandBufferRelease(commands);
  GetProcs().commandEncoderRelease(encoder);
  GetProcs().bufferRelease(result.buffer);

  return true;
}

bool GPUQueue::CopyContentFromGPU(StaticBitmapImage* image,
                                  const WGPUOrigin3D& origin,
                                  const WGPUExtent3D& copy_size,
                                  const WGPUTextureCopyView& destination) {
  WGPUTexture src_texture =
      produce_dawn_texture_handler_->ProduceDawnTextureFromImageBitmap(image);
  // Failed to produceDawnTexture.
  if (!src_texture) {
    return false;
  }

  WGPUTextureCopyView src;
  src.nextInChain = nullptr;
  src.texture = src_texture;
  src.mipLevel = 0;
  src.arrayLayer = 0;
  src.origin = origin;

  WGPUCommandEncoder encoder =
      GetProcs().deviceCreateCommandEncoder(device_->GetHandle(), nullptr);
  GetProcs().commandEncoderCopyTextureToTexture(encoder, &src, &destination,
                                                &copy_size);
  WGPUCommandBuffer commands =
      GetProcs().commandEncoderFinish(encoder, nullptr);

  // Don't need to add fence after this submit. Because if user want to use the
  // texture to do copy or render, it will trigger another queue submit. Dawn
  // will insert the necessary resource transitions.
  GetProcs().queueSubmit(GetHandle(), 1, &commands);

  // Release intermediate resources.
  GetProcs().commandBufferRelease(commands);
  GetProcs().commandEncoderRelease(encoder);

  produce_dawn_texture_handler_->FinishDawnTextureFromImageBitmapAccess();
  return true;
}

}  // namespace blink
