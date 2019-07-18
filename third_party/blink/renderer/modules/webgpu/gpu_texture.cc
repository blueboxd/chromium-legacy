// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webgpu/gpu_texture.h"

#include "third_party/blink/renderer/modules/webgpu/dawn_conversions.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_device.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_descriptor.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view.h"
#include "third_party/blink/renderer/modules/webgpu/gpu_texture_view_descriptor.h"

namespace blink {

namespace {

DawnTextureDescriptor AsDawnType(const GPUTextureDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnTextureDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.usage = static_cast<DawnTextureUsageBit>(webgpu_desc->usage());
  dawn_desc.dimension =
      AsDawnEnum<DawnTextureDimension>(webgpu_desc->dimension());
  dawn_desc.size = AsDawnType(webgpu_desc->size());
  dawn_desc.arrayLayerCount = webgpu_desc->arrayLayerCount();
  dawn_desc.format = AsDawnEnum<DawnTextureFormat>(webgpu_desc->format());
  dawn_desc.mipLevelCount = webgpu_desc->mipLevelCount();
  dawn_desc.sampleCount = webgpu_desc->sampleCount();

  return dawn_desc;
}

DawnTextureViewDescriptor AsDawnType(
    const GPUTextureViewDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnTextureViewDescriptor dawn_desc = {};
  dawn_desc.nextInChain = nullptr;
  dawn_desc.format = AsDawnEnum<DawnTextureFormat>(webgpu_desc->format());
  dawn_desc.dimension =
      AsDawnEnum<DawnTextureViewDimension>(webgpu_desc->dimension());
  dawn_desc.baseMipLevel = webgpu_desc->baseMipLevel();
  dawn_desc.mipLevelCount = webgpu_desc->mipLevelCount();
  dawn_desc.baseArrayLayer = webgpu_desc->baseArrayLayer();
  dawn_desc.arrayLayerCount = webgpu_desc->arrayLayerCount();

  return dawn_desc;
}

}  // anonymous namespace

// static
GPUTexture* GPUTexture::Create(GPUDevice* device,
                               const GPUTextureDescriptor* webgpu_desc) {
  DCHECK(device);
  DCHECK(webgpu_desc);
  DawnTextureDescriptor dawn_desc = AsDawnType(webgpu_desc);

  return MakeGarbageCollected<GPUTexture>(
      device,
      device->GetProcs().deviceCreateTexture(device->GetHandle(), &dawn_desc));
}

GPUTexture::GPUTexture(GPUDevice* device, DawnTexture texture)
    : DawnObject<DawnTexture>(device, texture) {}

GPUTexture::~GPUTexture() {
  if (IsDawnControlClientDestroyed()) {
    return;
  }
  GetProcs().textureRelease(GetHandle());
}

GPUTextureView* GPUTexture::createView(
    const GPUTextureViewDescriptor* webgpu_desc) {
  DCHECK(webgpu_desc);

  DawnTextureViewDescriptor dawn_desc = AsDawnType(webgpu_desc);
  return GPUTextureView::Create(
      device_, GetProcs().textureCreateView(GetHandle(), &dawn_desc));
}

GPUTextureView* GPUTexture::createDefaultView() {
  return GPUTextureView::Create(
      device_, GetProcs().textureCreateDefaultView(GetHandle()));
}

void GPUTexture::destroy() {
  GetProcs().textureDestroy(GetHandle());
}

}  // namespace blink
