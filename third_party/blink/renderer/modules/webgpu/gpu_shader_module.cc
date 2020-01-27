// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_shader_module.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_shader_module_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

// static
GPUShaderModule* GPUShaderModule::Create(
    GPUDevice* device,
    const GPUShaderModuleDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);
  uint32_t byte_length = 0;
  if (!base::CheckedNumeric<uint32_t>(
           webgpu_desc->code().View()->lengthAsSizeT())
           .AssignIfValid(&byte_length)) {
    exception_state.ThrowRangeError(
        "The provided ArrayBuffer exceeds the maximum supported size "
        "(4294967295)");
    return nullptr;
  }

  WGPUShaderModuleDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.code = webgpu_desc->code().View()->DataMaybeShared();
  dawn_desc.codeSize = byte_length;
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  return MakeGarbageCollected<GPUShaderModule>(
      device, device->GetProcs().deviceCreateShaderModule(device->GetHandle(),
                                                          &dawn_desc));
}

GPUShaderModule::GPUShaderModule(GPUDevice* device,
                                 WGPUShaderModule shader_module)
    : DawnObject<WGPUShaderModule>(device, shader_module) {}

GPUShaderModule::~GPUShaderModule() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().shaderModuleRelease(GetHandle());
}

}  // namespace blink
