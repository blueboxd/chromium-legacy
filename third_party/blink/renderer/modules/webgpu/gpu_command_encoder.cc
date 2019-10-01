// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_command_encoder.h"

#include "third_party/blink/renderer/bindings/modules/v8/double_sequence_or_gpu_color_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer_copy_view.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_encoder_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pass_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_compute_pass_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_origin_3d.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_color_attachment_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_depth_stencil_attachment_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_encoder.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_copy_view.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"

namespace blink {

bool ValidateCopySize(UnsignedLongSequenceOrGPUExtent3DDict& copy_size,
                      ExceptionState& exception_state) {
  if (copy_size.IsUnsignedLongSequence() &&
      copy_size.GetAsUnsignedLongSequence().size() != 3) {
    exception_state.ThrowRangeError("copySize length must be 3");
    return false;
  }
  return true;
}

DawnRenderPassColorAttachmentDescriptor AsDawnType(
    const GPURenderPassColorAttachmentDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnRenderPassColorAttachmentDescriptor dawn_desc = {};
  dawn_desc.attachment = webgpu_desc->attachment()->GetHandle();
  dawn_desc.resolveTarget = webgpu_desc->resolveTarget()
                                ? webgpu_desc->resolveTarget()->GetHandle()
                                : nullptr;

  if (webgpu_desc->loadValue().IsGPULoadOp()) {
    const WTF::String& gpuLoadOp = webgpu_desc->loadValue().GetAsGPULoadOp();
    dawn_desc.loadOp = AsDawnEnum<DawnLoadOp>(gpuLoadOp);

  } else if (webgpu_desc->loadValue().IsDoubleSequence()) {
    const Vector<double>& gpuColor =
        webgpu_desc->loadValue().GetAsDoubleSequence();
    dawn_desc.loadOp = DAWN_LOAD_OP_CLEAR;
    dawn_desc.clearColor = AsDawnColor(gpuColor);

  } else if (webgpu_desc->loadValue().IsGPUColorDict()) {
    const GPUColorDict* gpuColor = webgpu_desc->loadValue().GetAsGPUColorDict();
    dawn_desc.loadOp = DAWN_LOAD_OP_CLEAR;
    dawn_desc.clearColor = AsDawnType(gpuColor);

  } else {
    NOTREACHED();
  }

  dawn_desc.storeOp = AsDawnEnum<DawnStoreOp>(webgpu_desc->storeOp());

  return dawn_desc;
}

namespace {

DawnRenderPassDepthStencilAttachmentDescriptor AsDawnType(
    const GPURenderPassDepthStencilAttachmentDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnRenderPassDepthStencilAttachmentDescriptor dawn_desc = {};
  dawn_desc.attachment = webgpu_desc->attachment()->GetHandle();

  if (webgpu_desc->depthLoadValue().IsGPULoadOp()) {
    const WTF::String& gpuLoadOp =
        webgpu_desc->depthLoadValue().GetAsGPULoadOp();
    dawn_desc.depthLoadOp = AsDawnEnum<DawnLoadOp>(gpuLoadOp);
    dawn_desc.clearDepth = 1.0f;

  } else if (webgpu_desc->depthLoadValue().IsFloat()) {
    dawn_desc.depthLoadOp = DAWN_LOAD_OP_CLEAR;
    dawn_desc.clearDepth = webgpu_desc->depthLoadValue().GetAsFloat();

  } else {
    NOTREACHED();
  }

  dawn_desc.depthStoreOp = AsDawnEnum<DawnStoreOp>(webgpu_desc->depthStoreOp());

  if (webgpu_desc->stencilLoadValue().IsGPULoadOp()) {
    const WTF::String& gpuLoadOp =
        webgpu_desc->stencilLoadValue().GetAsGPULoadOp();
    dawn_desc.stencilLoadOp = AsDawnEnum<DawnLoadOp>(gpuLoadOp);
    dawn_desc.clearStencil = 0;

  } else if (webgpu_desc->stencilLoadValue().IsUnsignedLong()) {
    dawn_desc.stencilLoadOp = DAWN_LOAD_OP_CLEAR;
    dawn_desc.clearStencil =
        webgpu_desc->stencilLoadValue().GetAsUnsignedLong();

  } else {
    NOTREACHED();
  }

  dawn_desc.stencilStoreOp =
      AsDawnEnum<DawnStoreOp>(webgpu_desc->stencilStoreOp());

  return dawn_desc;
}

DawnBufferCopyView AsDawnType(const GPUBufferCopyView* webgpu_view) {
  DCHECK(webgpu_view);
  DCHECK(webgpu_view->buffer());

  DawnBufferCopyView dawn_view;
  dawn_view.nextInChain = nullptr;
  dawn_view.buffer = webgpu_view->buffer()->GetHandle();
  dawn_view.offset = webgpu_view->offset();
  dawn_view.rowPitch = webgpu_view->rowPitch();
  dawn_view.imageHeight = webgpu_view->imageHeight();

  return dawn_view;
}

DawnTextureCopyView AsDawnType(const GPUTextureCopyView* webgpu_view) {
  DCHECK(webgpu_view);
  DCHECK(webgpu_view->texture());

  DawnTextureCopyView dawn_view;
  dawn_view.nextInChain = nullptr;
  dawn_view.texture = webgpu_view->texture()->GetHandle();
  dawn_view.mipLevel = webgpu_view->mipLevel();
  dawn_view.arrayLayer = webgpu_view->arrayLayer();
  dawn_view.origin = AsDawnType(webgpu_view->origin());

  return dawn_view;
}

DawnCommandEncoderDescriptor AsDawnType(
    const GPUCommandEncoderDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnCommandEncoderDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;

  return dawn_desc;
}

}  // anonymous namespace

// static
GPUCommandEncoder* GPUCommandEncoder::Create(
    GPUDevice* device,
    const GPUCommandEncoderDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);
  ALLOW_UNUSED_LOCAL(webgpu_desc);

  DawnCommandEncoderDescriptor dawn_desc = {};
  const DawnCommandEncoderDescriptor* dawn_desc_ptr = nullptr;
  if (webgpu_desc) {
    dawn_desc = AsDawnType(webgpu_desc);
    dawn_desc_ptr = &dawn_desc;
  }

  return MakeGarbageCollected<GPUCommandEncoder>(
      device, device->GetProcs().deviceCreateCommandEncoder(device->GetHandle(),
                                                            dawn_desc_ptr));
}

GPUCommandEncoder::GPUCommandEncoder(GPUDevice* device,
                                     DawnCommandEncoder command_encoder)
    : DawnObject<DawnCommandEncoder>(device, command_encoder) {}

GPUCommandEncoder::~GPUCommandEncoder() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().commandEncoderRelease(GetHandle());
}

GPURenderPassEncoder* GPUCommandEncoder::beginRenderPass(
    const GPURenderPassDescriptor* descriptor,
    ExceptionState& exception_state) {
  DCHECK(descriptor);

  uint32_t color_attachment_count =
      static_cast<uint32_t>(descriptor->colorAttachments().size());

  // Check loadValue color is correctly formatted before further processing.
  for (wtf_size_t i = 0; i < color_attachment_count; ++i) {
    const GPURenderPassColorAttachmentDescriptor* color_attachment =
        descriptor->colorAttachments()[i];
    const GPULoadOpOrDoubleSequenceOrGPUColorDict load_value =
        color_attachment->loadValue();

    if (load_value.IsDoubleSequence() &&
        load_value.GetAsDoubleSequence().size() != 4) {
      exception_state.ThrowRangeError("loadValue color size must be 4");
      return nullptr;
    }
  }

  DawnRenderPassDescriptor dawn_desc = {};
  dawn_desc.colorAttachmentCount = color_attachment_count;
  dawn_desc.colorAttachments = nullptr;

  std::unique_ptr<DawnRenderPassColorAttachmentDescriptor[]> color_attachments;

  if (color_attachment_count > 0) {
    color_attachments = AsDawnType(descriptor->colorAttachments());
    dawn_desc.colorAttachments = color_attachments.get();
  }

  DawnRenderPassDepthStencilAttachmentDescriptor depthStencilAttachment = {};
  if (descriptor->hasDepthStencilAttachment()) {
    depthStencilAttachment = AsDawnType(descriptor->depthStencilAttachment());
    dawn_desc.depthStencilAttachment = &depthStencilAttachment;
  } else {
    dawn_desc.depthStencilAttachment = nullptr;
  }

  return GPURenderPassEncoder::Create(
      device_,
      GetProcs().commandEncoderBeginRenderPass(GetHandle(), &dawn_desc));
}

GPUComputePassEncoder* GPUCommandEncoder::beginComputePass(
    const GPUComputePassDescriptor* descriptor) {
  DawnComputePassDescriptor dawn_desc = {};
  if (descriptor->hasLabel()) {
    dawn_desc.label = descriptor->label().Utf8().data();
  }

  return GPUComputePassEncoder::Create(
      device_,
      GetProcs().commandEncoderBeginComputePass(GetHandle(), &dawn_desc));
}

void GPUCommandEncoder::copyBufferToBuffer(GPUBuffer* src,
                                           uint64_t src_offset,
                                           GPUBuffer* dst,
                                           uint64_t dst_offset,
                                           uint64_t size) {
  DCHECK(src);
  DCHECK(dst);
  GetProcs().commandEncoderCopyBufferToBuffer(GetHandle(), src->GetHandle(),
                                              src_offset, dst->GetHandle(),
                                              dst_offset, size);
}

void GPUCommandEncoder::copyBufferToTexture(
    GPUBufferCopyView* source,
    GPUTextureCopyView* destination,
    UnsignedLongSequenceOrGPUExtent3DDict& copy_size,
    ExceptionState& exception_state) {
  if (!ValidateCopySize(copy_size, exception_state)) {
    return;
  }

  DawnBufferCopyView dawn_source = AsDawnType(source);
  DawnTextureCopyView dawn_destination = AsDawnType(destination);
  DawnExtent3D dawn_copy_size = AsDawnType(&copy_size);

  GetProcs().commandEncoderCopyBufferToTexture(
      GetHandle(), &dawn_source, &dawn_destination, &dawn_copy_size);
}

void GPUCommandEncoder::copyTextureToBuffer(
    GPUTextureCopyView* source,
    GPUBufferCopyView* destination,
    UnsignedLongSequenceOrGPUExtent3DDict& copy_size,
    ExceptionState& exception_state) {
  if (!ValidateCopySize(copy_size, exception_state)) {
    return;
  }

  DawnTextureCopyView dawn_source = AsDawnType(source);
  DawnBufferCopyView dawn_destination = AsDawnType(destination);
  DawnExtent3D dawn_copy_size = AsDawnType(&copy_size);

  GetProcs().commandEncoderCopyTextureToBuffer(
      GetHandle(), &dawn_source, &dawn_destination, &dawn_copy_size);
}

void GPUCommandEncoder::copyTextureToTexture(
    GPUTextureCopyView* source,
    GPUTextureCopyView* destination,
    UnsignedLongSequenceOrGPUExtent3DDict& copy_size,
    ExceptionState& exception_state) {
  if (!ValidateCopySize(copy_size, exception_state)) {
    return;
  }

  DawnTextureCopyView dawn_source = AsDawnType(source);
  DawnTextureCopyView dawn_destination = AsDawnType(destination);
  DawnExtent3D dawn_copy_size = AsDawnType(&copy_size);

  GetProcs().commandEncoderCopyTextureToTexture(
      GetHandle(), &dawn_source, &dawn_destination, &dawn_copy_size);
}

void GPUCommandEncoder::pushDebugGroup(String groupLabel) {
  GetProcs().commandEncoderPushDebugGroup(GetHandle(),
                                          groupLabel.Utf8().data());
}

void GPUCommandEncoder::popDebugGroup() {
  GetProcs().commandEncoderPopDebugGroup(GetHandle());
}

void GPUCommandEncoder::insertDebugMarker(String markerLabel) {
  GetProcs().commandEncoderInsertDebugMarker(GetHandle(),
                                             markerLabel.Utf8().data());
}

GPUCommandBuffer* GPUCommandEncoder::finish(
    const GPUCommandBufferDescriptor* descriptor) {
  DawnCommandBufferDescriptor dawn_desc = {};
  if (descriptor->hasLabel()) {
    dawn_desc.label = descriptor->label().Utf8().data();
  }

  return GPUCommandBuffer::Create(
      device_, GetProcs().commandEncoderFinish(GetHandle(), &dawn_desc));
}

}  // namespace blink
