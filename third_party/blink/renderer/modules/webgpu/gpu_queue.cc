// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_queue.h"

#include "gpu/command_buffer/client/webgpu_interface.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_extent_3d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_origin_2d_dict.h"
#include "third_party/blink/renderer/bindings/modules/v8/unsigned_long_sequence_or_gpu_origin_3d_dict.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_command_buffer.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_fence.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_fence_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_copy_view.h"

namespace blink {

// static
GPUQueue* GPUQueue::Create(GPUDevice* device, WGPUQueue queue) {
  return MakeGarbageCollected<GPUQueue>(device, queue);
}

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

  return GPUFence::Create(device_,
                          GetProcs().queueCreateFence(GetHandle(), &desc));
}

// TODO(shaobo.yan@intel.com): Implement this function
void GPUQueue::copyImageBitmapToTexture(
    GPUImageBitmapCopyView* source,
    GPUTextureCopyView* destination,
    UnsignedLongSequenceOrGPUExtent3DDict& copy_size,
    ExceptionState& exception_state) {
  NOTIMPLEMENTED();

  // TODO(shaobo.yan@intel.com): Validate source copy size, format and dest copy
  // size format
  // TODO(shaobo.yan@intel.com): Implement sharing to staging texture path.
  return;
}

}  // namespace blink
