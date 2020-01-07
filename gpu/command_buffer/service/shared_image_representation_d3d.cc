// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation_d3d.h"

#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/service/shared_image_backing_d3d.h"

namespace gpu {

SharedImageRepresentationGLTextureD3D::SharedImageRepresentationGLTextureD3D(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    gles2::Texture* texture)
    : SharedImageRepresentationGLTexture(manager, backing, tracker),
      texture_(texture) {}

gles2::Texture* SharedImageRepresentationGLTextureD3D::GetTexture() {
  return texture_;
}

SharedImageRepresentationGLTextureD3D::
    ~SharedImageRepresentationGLTextureD3D() = default;

SharedImageRepresentationGLTexturePassthroughD3D::
    SharedImageRepresentationGLTexturePassthroughD3D(
        SharedImageManager* manager,
        SharedImageBacking* backing,
        MemoryTypeTracker* tracker,
        scoped_refptr<gles2::TexturePassthrough> texture_passthrough)
    : SharedImageRepresentationGLTexturePassthrough(manager, backing, tracker),
      texture_passthrough_(std::move(texture_passthrough)) {}

const scoped_refptr<gles2::TexturePassthrough>&
SharedImageRepresentationGLTexturePassthroughD3D::GetTexturePassthrough() {
  return texture_passthrough_;
}

SharedImageRepresentationGLTexturePassthroughD3D::
    ~SharedImageRepresentationGLTexturePassthroughD3D() = default;

bool SharedImageRepresentationGLTexturePassthroughD3D::BeginAccess(
    GLenum mode) {
  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());
  return d3d_image_backing->BeginAccessD3D11();
}

void SharedImageRepresentationGLTexturePassthroughD3D::EndAccess() {
  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());
  d3d_image_backing->EndAccessD3D11();
}

#if BUILDFLAG(USE_DAWN)
SharedImageRepresentationDawnD3D::SharedImageRepresentationDawnD3D(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* tracker,
    WGPUDevice device)
    : SharedImageRepresentationDawn(manager, backing, tracker),
      device_(device),
      dawn_procs_(dawn_native::GetProcs()) {
  DCHECK(device_);

  // Keep a reference to the device so that it stays valid (it might become
  // lost in which case operations will be noops).
  dawn_procs_.deviceReference(device_);
}

SharedImageRepresentationDawnD3D::~SharedImageRepresentationDawnD3D() {
  EndAccess();
  dawn_procs_.deviceRelease(device_);
}

WGPUTexture SharedImageRepresentationDawnD3D::BeginAccess(
    WGPUTextureUsage usage) {
  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());

  const HANDLE shared_handle = d3d_image_backing->GetSharedHandle();
  const viz::ResourceFormat viz_resource_format = d3d_image_backing->format();
  WGPUTextureFormat wgpu_format = viz::ToWGPUFormat(viz_resource_format);
  if (wgpu_format == WGPUTextureFormat_Undefined) {
    DLOG(ERROR) << "Unsupported viz format found: " << viz_resource_format;
    return nullptr;
  }

  uint64_t shared_mutex_acquire_key;
  if (!d3d_image_backing->BeginAccessD3D12(&shared_mutex_acquire_key)) {
    return nullptr;
  }

  WGPUTextureDescriptor desc;
  desc.nextInChain = nullptr;
  desc.format = wgpu_format;
  desc.usage = usage;
  desc.dimension = WGPUTextureDimension_2D;
  desc.size = {size().width(), size().height(), 1};
  desc.arrayLayerCount = 1;
  desc.mipLevelCount = 1;
  desc.sampleCount = 1;

  texture_ = dawn_native::d3d12::WrapSharedHandle(device_, &desc, shared_handle,
                                                  shared_mutex_acquire_key);
  if (texture_) {
    // Keep a reference to the texture so that it stays valid (its content
    // might be destroyed).
    dawn_procs_.textureReference(texture_);

    // Assume that the user of this representation will write to the texture
    // so set the cleared flag so that other representations don't overwrite
    // the result.
    // TODO(cwallez@chromium.org): This is incorrect and allows reading
    // uninitialized data. When !IsCleared we should tell dawn_native to
    // consider the texture lazy-cleared. crbug.com/1036080
    SetCleared();
  } else {
    d3d_image_backing->EndAccessD3D12();
  }

  return texture_;
}

void SharedImageRepresentationDawnD3D::EndAccess() {
  if (!texture_) {
    return;
  }

  SharedImageBackingD3D* d3d_image_backing =
      static_cast<SharedImageBackingD3D*>(backing());

  // TODO(cwallez@chromium.org): query dawn_native to know if the texture was
  // cleared and set IsCleared appropriately.

  // All further operations on the textures are errors (they would be racy
  // with other backings).
  dawn_procs_.textureDestroy(texture_);

  dawn_procs_.textureRelease(texture_);
  texture_ = nullptr;

  d3d_image_backing->EndAccessD3D12();
}
#endif  // BUILDFLAG(USE_DAWN)

}  // namespace gpu
