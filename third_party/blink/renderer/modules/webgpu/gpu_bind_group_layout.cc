// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"

namespace blink {

DawnBindGroupLayoutBinding AsDawnType(
    const GPUBindGroupLayoutBinding* webgpu_binding) {
  DawnBindGroupLayoutBinding dawn_binding;

  dawn_binding.binding = webgpu_binding->binding();
  dawn_binding.type = AsDawnEnum<DawnBindingType>(webgpu_binding->type());
  dawn_binding.visibility =
      AsDawnEnum<DawnShaderStageBit>(webgpu_binding->visibility());
  dawn_binding.dynamic = webgpu_binding->dynamic();

  return dawn_binding;
}

// static
GPUBindGroupLayout* GPUBindGroupLayout::Create(
    GPUDevice* device,
    const GPUBindGroupLayoutDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  uint32_t binding_count =
      static_cast<uint32_t>(webgpu_desc->bindings().size());

  std::unique_ptr<DawnBindGroupLayoutBinding[]> bindings =
      binding_count != 0 ? AsDawnType(webgpu_desc->bindings()) : nullptr;

  DawnBindGroupLayoutDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.bindingCount = binding_count;
  dawn_desc.bindings = bindings.get();

  return MakeGarbageCollected<GPUBindGroupLayout>(
      device, device->GetProcs().deviceCreateBindGroupLayout(
                  device->GetHandle(), &dawn_desc));
}

GPUBindGroupLayout::GPUBindGroupLayout(GPUDevice* device,
                                       DawnBindGroupLayout bind_group_layout)
    : DawnObject<DawnBindGroupLayout>(device, bind_group_layout) {}

GPUBindGroupLayout::~GPUBindGroupLayout() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().bindGroupLayoutRelease(GetHandle());
}

}  // namespace blink
