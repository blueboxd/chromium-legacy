// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_sampler.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_sampler_descriptor.h"

namespace blink {

namespace {

DawnSamplerDescriptor AsDawnType(const GPUSamplerDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnSamplerDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.addressModeU =
      AsDawnEnum<DawnAddressMode>(webgpu_desc->addressModeU());
  dawn_desc.addressModeV =
      AsDawnEnum<DawnAddressMode>(webgpu_desc->addressModeV());
  dawn_desc.addressModeW =
      AsDawnEnum<DawnAddressMode>(webgpu_desc->addressModeW());
  dawn_desc.magFilter = AsDawnEnum<DawnFilterMode>(webgpu_desc->magFilter());
  dawn_desc.minFilter = AsDawnEnum<DawnFilterMode>(webgpu_desc->minFilter());
  dawn_desc.mipmapFilter =
      AsDawnEnum<DawnFilterMode>(webgpu_desc->mipmapFilter());
  dawn_desc.lodMinClamp = webgpu_desc->lodMinClamp();
  dawn_desc.lodMaxClamp = webgpu_desc->lodMaxClamp();
  dawn_desc.compare =
      AsDawnEnum<DawnCompareFunction>(webgpu_desc->compareFunction());

  return dawn_desc;
}

}  // anonymous namespace

// static
GPUSampler* GPUSampler::Create(GPUDevice* device,
                               const GPUSamplerDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);
  DawnSamplerDescriptor dawn_desc = AsDawnType(webgpu_desc);
  return MakeGarbageCollected<GPUSampler>(
      device,
      device->GetProcs().deviceCreateSampler(device->GetHandle(), &dawn_desc));
}

GPUSampler::GPUSampler(GPUDevice* device, DawnSampler sampler)
    : DawnObject<DawnSampler>(device, sampler) {}

GPUSampler::~GPUSampler() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().samplerRelease(GetHandle());
}

}  // namespace blink
