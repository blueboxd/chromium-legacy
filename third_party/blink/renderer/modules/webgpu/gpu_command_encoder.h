// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_

#include "third_party/blink/renderer/modules/webgpu/dawn_object.h"

namespace blink {

class GPUBuffer;
class GPUBufferCopyView;
class GPUCommandBuffer;
class GPUCommandEncoderDescriptor;
class GPUComputePassEncoder;
class GPUExtent3D;
class GPURenderPassDescriptor;
class GPURenderPassEncoder;
class GPUTextureCopyView;

class GPUCommandEncoder : public DawnObject<DawnCommandEncoder> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static GPUCommandEncoder* Create(
      GPUDevice* device,
      const GPUCommandEncoderDescriptor* webgpu_desc);
  explicit GPUCommandEncoder(GPUDevice* device,
                             DawnCommandEncoder command_encoder);
  ~GPUCommandEncoder() override;

  // gpu_command_encoder.idl
  GPURenderPassEncoder* beginRenderPass(
      const GPURenderPassDescriptor* descriptor);
  GPUComputePassEncoder* beginComputePass();
  void copyBufferToBuffer(GPUBuffer* src,
                          uint64_t src_offset,
                          GPUBuffer* dst,
                          uint64_t dst_offset,
                          uint64_t size);
  void copyBufferToTexture(GPUBufferCopyView* source,
                           GPUTextureCopyView* destination,
                           GPUExtent3D* copy_size);
  void copyTextureToBuffer(GPUTextureCopyView* source,
                           GPUBufferCopyView* destination,
                           GPUExtent3D* copy_size);
  void copyTextureToTexture(GPUTextureCopyView* source,
                            GPUTextureCopyView* destination,
                            GPUExtent3D* copy_size);
  void pushDebugGroup(String groupLabel);
  void popDebugGroup();
  void insertDebugMarker(String markerLabel);
  GPUCommandBuffer* finish();

 private:
  DISALLOW_COPY_AND_ASSIGN(GPUCommandEncoder);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBGPU_GPU_COMMAND_ENCODER_H_
