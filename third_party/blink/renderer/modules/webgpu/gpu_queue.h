// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class ExceptionState;
class GPUCommandBuffer;
class GPUFence;
class GPUFenceDescriptor;
class GPUImageBitmapCopyView;
class GPUTextureCopyView;
class UnsignedLongSequenceOrGPUExtent3DDict;

class GPUQueue : public DawnObject<WGPUQueue> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUQueue* Create(GPUDevice* device, WGPUQueue queue);
  explicit GPUQueue(GPUDevice* device, WGPUQueue queue);
  ~GPUQueue() override;

  // gpu_queue.idl
  void submit(const HeapVector<Member<GPUCommandBuffer>>& buffers);
  void signal(GPUFence* fence, uint64_t signal_value);
  GPUFence* createFence(const GPUFenceDescriptor* descriptor);
  void copyImageBitmapToTexture(GPUImageBitmapCopyView* source,
                                GPUTextureCopyView* destination,
                                UnsignedLongSequenceOrGPUExtent3DDict& copySize,
                                ExceptionState& exception_state);

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUQueue);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_
