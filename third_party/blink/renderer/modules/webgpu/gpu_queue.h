// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

class CanvasColorParams;
class DawnTextureFromImageBitmap;
class ExceptionState;
class GPUCommandBuffer;
class GPUFence;
class GPUFenceDescriptor;
class GPUImageBitmapCopyView;
class GPUTextureCopyView;
class StaticBitmapImage;
class UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict;

class GPUQueue : public DawnObject<WGPUQueue> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit GPUQueue(GPUDevice* device, WGPUQueue queue);
  ~GPUQueue() override;

  // gpu_queue.idl
  void submit(const HeapVector<Member<GPUCommandBuffer>>& buffers);
  void signal(GPUFence* fence, uint64_t signal_value);
  GPUFence* createFence(const GPUFenceDescriptor* descriptor);
  void copyImageBitmapToTexture(
      GPUImageBitmapCopyView* source,
      GPUTextureCopyView* destination,
      UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& copySize,
      ExceptionState& exception_state);

 private:
  bool CopyContentFromCPU(StaticBitmapImage* image,
                          const CanvasColorParams& color_params,
                          const WGPUOrigin3D& origin,
                          const WGPUExtent3D& copy_size,
                          const WGPUTextureCopyView& destination);
  bool CopyContentFromGPU(StaticBitmapImage* image,
                          const WGPUOrigin3D& origin,
                          const WGPUExtent3D& copy_size,
                          const WGPUTextureCopyView& destination);

  scoped_refptr<DawnTextureFromImageBitmap> produce_dawn_texture_handler_;

  DISALLOW_COPY_AND_ASSIGN(GPUQueue);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_QUEUE_H_
