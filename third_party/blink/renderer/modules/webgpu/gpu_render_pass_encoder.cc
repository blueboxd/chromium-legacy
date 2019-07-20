// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_render_pass_encoder.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_color.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_render_pipeline.h"

namespace blink {

// static
GPURenderPassEncoder* GPURenderPassEncoder::Create(
    GPUDevice* device,
    DawnRenderPassEncoder render_pass_encoder) {
  return MakeGarbageCollected<GPURenderPassEncoder>(device,
                                                    render_pass_encoder);
}

GPURenderPassEncoder::GPURenderPassEncoder(
    GPUDevice* device,
    DawnRenderPassEncoder render_pass_encoder)
    : DawnObject<DawnRenderPassEncoder>(device, render_pass_encoder) {}

GPURenderPassEncoder::~GPURenderPassEncoder() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().renderPassEncoderRelease(GetHandle());
}

void GPURenderPassEncoder::setBindGroup(
    uint32_t index,
    GPUBindGroup* bindGroup,
    const Vector<uint64_t>& dynamicOffsets) {
  GetProcs().renderPassEncoderSetBindGroup(
      GetHandle(), index, bindGroup->GetHandle(), dynamicOffsets.size(),
      dynamicOffsets.data());
}

void GPURenderPassEncoder::setPipeline(GPURenderPipeline* pipeline) {
  GetProcs().renderPassEncoderSetPipeline(GetHandle(), pipeline->GetHandle());
}

void GPURenderPassEncoder::setBlendColor(GPUColor* color) {
  DawnColor dawn_color = AsDawnType(color);
  GetProcs().renderPassEncoderSetBlendColor(GetHandle(), &dawn_color);
}

void GPURenderPassEncoder::setStencilReference(uint32_t reference) {
  GetProcs().renderPassEncoderSetStencilReference(GetHandle(), reference);
}

void GPURenderPassEncoder::setViewport(float x,
                                       float y,
                                       float width,
                                       float height,
                                       float minDepth,
                                       float maxDepth) {
  GetProcs().renderPassEncoderSetViewport(GetHandle(), x, y, width, height,
                                          minDepth, maxDepth);
}

void GPURenderPassEncoder::setScissorRect(uint32_t x,
                                          uint32_t y,
                                          uint32_t width,
                                          uint32_t height) {
  GetProcs().renderPassEncoderSetScissorRect(GetHandle(), x, y, width, height);
}

void GPURenderPassEncoder::setIndexBuffer(GPUBuffer* buffer, uint64_t offset) {
  GetProcs().renderPassEncoderSetIndexBuffer(GetHandle(), buffer->GetHandle(),
                                             offset);
}

void GPURenderPassEncoder::setVertexBuffers(
    uint32_t startSlot,
    const HeapVector<Member<GPUBuffer>>& buffers,
    const Vector<uint64_t>& offsets,
    ExceptionState& exception_state) {
  if (buffers.size() != offsets.size()) {
    exception_state.ThrowRangeError(
        "buffers array and offsets array must be the same length");
    return;
  }

  std::unique_ptr<DawnBuffer[]> dawn_buffers = AsDawnType(buffers);

  GetProcs().renderPassEncoderSetVertexBuffers(
      GetHandle(), startSlot, buffers.size(), dawn_buffers.get(),
      offsets.data());
}

void GPURenderPassEncoder::draw(uint32_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t firstInstance) {
  GetProcs().renderPassEncoderDraw(GetHandle(), vertexCount, instanceCount,
                                   firstVertex, firstInstance);
}

void GPURenderPassEncoder::drawIndexed(uint32_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t baseVertex,
                                       uint32_t firstInstance) {
  GetProcs().renderPassEncoderDrawIndexed(GetHandle(), indexCount,
                                          instanceCount, firstIndex, baseVertex,
                                          firstInstance);
}

void GPURenderPassEncoder::endPass() {
  GetProcs().renderPassEncoderEndPass(GetHandle());
}

}  // namespace blink
