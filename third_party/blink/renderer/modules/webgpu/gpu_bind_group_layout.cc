// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_bind_group_layout.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_bind_group_layout_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_bind_group_layout_entry.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

WGPUBindGroupLayoutEntry AsDawnType(
    const GPUBindGroupLayoutEntry* webgpu_binding) {
  WGPUBindGroupLayoutEntry dawn_binding = {};

  dawn_binding.binding = webgpu_binding->binding();
  dawn_binding.type = AsDawnEnum<WGPUBindingType>(webgpu_binding->type());
  dawn_binding.visibility =
      AsDawnEnum<WGPUShaderStage>(webgpu_binding->visibility());
  dawn_binding.textureDimension =
      AsDawnEnum<WGPUTextureViewDimension>(webgpu_binding->textureDimension());
  dawn_binding.textureComponentType = AsDawnEnum<WGPUTextureComponentType>(
      webgpu_binding->textureComponentType());
  dawn_binding.multisampled = webgpu_binding->multisampled();
  dawn_binding.hasDynamicOffset = webgpu_binding->hasDynamicOffset();

  return dawn_binding;
}

// static
GPUBindGroupLayout* GPUBindGroupLayout::Create(
    GPUDevice* device,
    const GPUBindGroupLayoutDescriptor* webgpu_desc,
    ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  if (webgpu_desc->hasBindings()) {
    device->AddConsoleWarning(
        "GPUBindGroupLayoutDescriptor.bindings is deprecated: renamed to "
        "entries");
  }

  uint32_t entry_count = 0;
  std::unique_ptr<WGPUBindGroupLayoutEntry[]> entries;
  if (webgpu_desc->hasEntries()) {
    entry_count = static_cast<uint32_t>(webgpu_desc->entries().size());
    entries = entry_count != 0 ? AsDawnType(webgpu_desc->entries()) : nullptr;
  } else {
    if (!webgpu_desc->hasBindings()) {
      exception_state.ThrowTypeError("required member entries is undefined.");
      return nullptr;
    }

    entry_count = static_cast<uint32_t>(webgpu_desc->bindings().size());
    entries = entry_count != 0 ? AsDawnType(webgpu_desc->bindings()) : nullptr;
  }

  WGPUBindGroupLayoutDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.bindingCount = entry_count;
  dawn_desc.bindings = entries.get();
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  return MakeGarbageCollected<GPUBindGroupLayout>(
      device, device->GetProcs().deviceCreateBindGroupLayout(
                  device->GetHandle(), &dawn_desc));
}

GPUBindGroupLayout::GPUBindGroupLayout(GPUDevice* device,
                                       WGPUBindGroupLayout bind_group_layout)
    : DawnObject<WGPUBindGroupLayout>(device, bind_group_layout) {}

GPUBindGroupLayout::~GPUBindGroupLayout() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().bindGroupLayoutRelease(GetHandle());
}

}  // namespace blink
