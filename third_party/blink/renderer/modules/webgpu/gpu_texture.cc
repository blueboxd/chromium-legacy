// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"

#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_gpu_texture_view_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"

namespace blink {

namespace {

WGPUTextureDescriptor AsDawnType(const GPUTextureDescriptor* webgpu_desc,
                                 GPUDevice* device) {
  DCHECK(webgpu_desc);

  WGPUTextureDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.usage = static_cast<WGPUTextureUsage>(webgpu_desc->usage());
  dawn_desc.dimension =
      AsDawnEnum<WGPUTextureDimension>(webgpu_desc->dimension());
  dawn_desc.size = AsDawnType(&webgpu_desc->size());

  if (webgpu_desc->hasArrayLayerCount()) {
    device->AddConsoleWarning("arrayLayerCount is deprecated: use size.depth");
    dawn_desc.arrayLayerCount = webgpu_desc->arrayLayerCount();
  } else {
    if (dawn_desc.dimension == WGPUTextureDimension_2D) {
      dawn_desc.arrayLayerCount = dawn_desc.size.depth;
      dawn_desc.size.depth = 1;
    }
  }

  dawn_desc.format = AsDawnEnum<WGPUTextureFormat>(webgpu_desc->format());
  dawn_desc.mipLevelCount = webgpu_desc->mipLevelCount();
  dawn_desc.sampleCount = webgpu_desc->sampleCount();
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  return dawn_desc;
}

WGPUTextureViewDescriptor AsDawnType(
    const GPUTextureViewDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUTextureViewDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.format = AsDawnEnum<WGPUTextureFormat>(webgpu_desc->format());
  dawn_desc.dimension =
      AsDawnEnum<WGPUTextureViewDimension>(webgpu_desc->dimension());
  dawn_desc.baseMipLevel = webgpu_desc->baseMipLevel();
  dawn_desc.mipLevelCount = webgpu_desc->mipLevelCount();
  dawn_desc.baseArrayLayer = webgpu_desc->baseArrayLayer();
  dawn_desc.arrayLayerCount = webgpu_desc->arrayLayerCount();
  dawn_desc.aspect = AsDawnEnum<WGPUTextureAspect>(webgpu_desc->aspect());
  if (webgpu_desc->hasLabel()) {
    dawn_desc.label = webgpu_desc->label().Utf8().data();
  }

  return dawn_desc;
}

}  // anonymous namespace

// static
GPUTexture* GPUTexture::Create(GPUDevice* device,
                               const GPUTextureDescriptor* webgpu_desc,
                               ExceptionState& exception_state) {
  DCHECK(device);
  DCHECK(webgpu_desc);

  // Check size is correctly formatted before further processing.
  const UnsignedLongEnforceRangeSequenceOrGPUExtent3DDict& size =
      webgpu_desc->size();
  if (size.IsUnsignedLongEnforceRangeSequence() &&
      size.GetAsUnsignedLongEnforceRangeSequence().size() != 3) {
    exception_state.ThrowRangeError("size length must be 3");
    return nullptr;
  }

  WGPUTextureDescriptor dawn_desc = AsDawnType(webgpu_desc, device);

  return MakeGarbageCollected<GPUTexture>(
      device,
      device->GetProcs().deviceCreateTexture(device->GetHandle(), &dawn_desc),
      dawn_desc.format);
}

GPUTexture::GPUTexture(GPUDevice* device,
                       WGPUTexture texture,
                       WGPUTextureFormat format)
    : DawnObject<WGPUTexture>(device, texture), format_(format) {}

GPUTexture::~GPUTexture() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().textureRelease(GetHandle());
}

GPUTextureView* GPUTexture::createView(
    const GPUTextureViewDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  WGPUTextureViewDescriptor dawn_desc = AsDawnType(webgpu_desc);
  return MakeGarbageCollected<GPUTextureView>(
      device_, GetProcs().textureCreateView(GetHandle(), &dawn_desc));
}

void GPUTexture::destroy() {
  GetProcs().textureDestroy(GetHandle());
}

}  // namespace blink
