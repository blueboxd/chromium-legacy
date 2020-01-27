// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_origin_2d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_origin_3d_dict.h"
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
    const UnsignedLongSequenceOrGPUOrigin2DDict* webgpu_origin) {
  DCHECK(webgpu_origin);

  WGPUOrigin3D dawn_origin = {};

  if (webgpu_origin->IsUnsignedLongSequence()) {
    const Vector<uint32_t>& webgpu_origin_sequence =
        webgpu_origin->GetAsUnsignedLongSequence();
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

}  // anonymous namespace

GPUQueue::GPUQueue(GPUDevice* device, WGPUQueue queue)
    : DawnObject<WGPUQueue>(device, queue) {}

GPUQueue::~GPUQueue() {
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
    UnsignedLongSequenceOrGPUExtent3DDict& copy_size,
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

  // TODO(shaobo.yan@intel.com): Implement GPU copy path
  if (image->IsTextureBacked()) {
    NOTIMPLEMENTED();
    exception_state.ThrowTypeError(
        "No support for texture backed imageBitmap yet.");
    return;
  }

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

  // Prepare for uploading CPU data.
  IntRect image_data_rect(origin_in_image_bitmap.x, origin_in_image_bitmap.y,
                          dawn_copy_size.width, dawn_copy_size.height);
  const CanvasColorParams& color_params =
      source->imageBitmap()->GetCanvasColorParams();
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
    exception_state.ThrowRangeError("Failed to copy image data");
    // Release the buffer.
    GetProcs().bufferRelease(result.buffer);
    return;
  }

  GetProcs().bufferUnmap(result.buffer);

  // Start a B2T copy to move contents from buffer to destination texture
  WGPUBufferCopyView dawn_intermediate;
  dawn_intermediate.nextInChain = nullptr;
  dawn_intermediate.buffer = result.buffer;
  dawn_intermediate.offset = 0;
  dawn_intermediate.rowPitch = info.wgpu_row_pitch;
  dawn_intermediate.imageHeight = source->imageBitmap()->height();

  WGPUTextureCopyView dawn_destination = AsDawnType(destination);

  WGPUCommandEncoderDescriptor encoder_desc = {};
  WGPUCommandEncoder encoder = GetProcs().deviceCreateCommandEncoder(
      device_->GetHandle(), &encoder_desc);
  GetProcs().commandEncoderCopyBufferToTexture(
      encoder, &dawn_intermediate, &dawn_destination, &dawn_copy_size);
  WGPUCommandBufferDescriptor dawn_desc_command = {};
  WGPUCommandBuffer commands =
      GetProcs().commandEncoderFinish(encoder, &dawn_desc_command);

  // Don't need to add fence after this submit. Because if user want to use the
  // texture to do copy or render, it will trigger another queue submit. Dawn
  // will insert the necessary resource transitions.
  GetProcs().queueSubmit(GetHandle(), 1, &commands);

  // Release intermediate resources.
  GetProcs().commandBufferRelease(commands);
  GetProcs().commandEncoderRelease(encoder);
  GetProcs().bufferRelease(result.buffer);
}

}  // namespace blink
