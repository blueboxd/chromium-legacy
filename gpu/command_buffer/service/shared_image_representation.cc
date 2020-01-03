// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/shared_image_representation.h"

#include "gpu/command_buffer/service/texture_manager.h"
#include "third_party/skia/include/core/SkPromiseImageTexture.h"

namespace gpu {

SharedImageRepresentation::SharedImageRepresentation(
    SharedImageManager* manager,
    SharedImageBacking* backing,
    MemoryTypeTracker* owning_tracker)
    : manager_(manager), backing_(backing), tracker_(owning_tracker) {
  DCHECK(tracker_);
  backing_->AddRef(this);
}

SharedImageRepresentation::~SharedImageRepresentation() {
  manager_->OnRepresentationDestroyed(backing_->mailbox(), this);
}

std::unique_ptr<SharedImageRepresentationGLTexture::ScopedAccess>
SharedImageRepresentationGLTextureBase::BeginScopedAccess(
    GLenum mode,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to access an uninitialized ShardImage";
    return nullptr;
  }

  if (!BeginAccess(mode))
    return nullptr;

  constexpr GLenum kReadAccess = 0x8AF6;
  if (mode == kReadAccess)
    backing()->OnReadSucceeded();
  else
    backing()->OnWriteSucceeded();

  return std::make_unique<ScopedAccess>(
      util::PassKey<SharedImageRepresentationGLTextureBase>(), this);
}

bool SharedImageRepresentationGLTextureBase::BeginAccess(GLenum mode) {
  return true;
}

bool SharedImageRepresentationSkia::SupportsMultipleConcurrentReadAccess() {
  return false;
}

void SharedImageRepresentationGLTexture::UpdateClearedStateOnEndAccess() {
  auto* texture = GetTexture();
  // Operations on the gles2::Texture may have cleared or uncleared it. Make
  // sure this state is reflected back in the SharedImage.
  gfx::Rect cleared_rect = texture->GetLevelClearedRect(texture->target(), 0);
  if (cleared_rect != ClearedRect())
    SetClearedRect(cleared_rect);
}

SharedImageRepresentationSkia::ScopedWriteAccess::ScopedWriteAccess(
    util::PassKey<SharedImageRepresentationSkia> /* pass_key */,
    SharedImageRepresentationSkia* representation,
    sk_sp<SkSurface> surface)
    : representation_(representation), surface_(std::move(surface)) {}

SharedImageRepresentationSkia::ScopedWriteAccess::~ScopedWriteAccess() {
  representation_->EndWriteAccess(std::move(surface_));
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
SharedImageRepresentationSkia::BeginScopedWriteAccess(
    int final_msaa_count,
    const SkSurfaceProps& surface_props,
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to write to an uninitialized ShardImage";
    return nullptr;
  }

  sk_sp<SkSurface> surface = BeginWriteAccess(final_msaa_count, surface_props,
                                              begin_semaphores, end_semaphores);
  if (!surface)
    return nullptr;

  return std::make_unique<ScopedWriteAccess>(
      util::PassKey<SharedImageRepresentationSkia>(), this, std::move(surface));
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedWriteAccess>
SharedImageRepresentationSkia::BeginScopedWriteAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores,
    AllowUnclearedAccess allow_uncleared) {
  return BeginScopedWriteAccess(
      0 /* final_msaa_count */,
      SkSurfaceProps(0 /* flags */, kUnknown_SkPixelGeometry), begin_semaphores,
      end_semaphores, allow_uncleared);
}

SharedImageRepresentationSkia::ScopedReadAccess::ScopedReadAccess(
    util::PassKey<SharedImageRepresentationSkia> /* pass_key */,
    SharedImageRepresentationSkia* representation,
    sk_sp<SkPromiseImageTexture> promise_image_texture)
    : representation_(representation),
      promise_image_texture_(std::move(promise_image_texture)) {}

SharedImageRepresentationSkia::ScopedReadAccess::~ScopedReadAccess() {
  representation_->EndReadAccess();
}

std::unique_ptr<SharedImageRepresentationSkia::ScopedReadAccess>
SharedImageRepresentationSkia::BeginScopedReadAccess(
    std::vector<GrBackendSemaphore>* begin_semaphores,
    std::vector<GrBackendSemaphore>* end_semaphores) {
  if (!IsCleared()) {
    LOG(ERROR) << "Attempt to read from an uninitialized ShardImage";
    return nullptr;
  }

  sk_sp<SkPromiseImageTexture> promise_image_texture =
      BeginReadAccess(begin_semaphores, end_semaphores);
  if (!promise_image_texture)
    return nullptr;

  return std::make_unique<ScopedReadAccess>(
      util::PassKey<SharedImageRepresentationSkia>(), this,
      std::move(promise_image_texture));
}

std::unique_ptr<SharedImageRepresentationOverlay::ScopedReadAccess>
SharedImageRepresentationOverlay::BeginScopedReadAccess(bool needs_gl_image) {
  if (!IsCleared()) {
    LOG(ERROR) << "Attempt to read from an uninitialized ShardImage";
    return nullptr;
  }

  BeginReadAccess();
  return std::make_unique<ScopedReadAccess>(
      util::PassKey<SharedImageRepresentationOverlay>(), this,
      needs_gl_image ? GetGLImage() : nullptr);
}

SharedImageRepresentationDawn::ScopedAccess::ScopedAccess(
    util::PassKey<SharedImageRepresentationDawn> /* pass_key */,
    SharedImageRepresentationDawn* representation,
    WGPUTexture texture)
    : representation_(representation), texture_(texture) {}

SharedImageRepresentationDawn::ScopedAccess::~ScopedAccess() {
  representation_->EndAccess();
}

std::unique_ptr<SharedImageRepresentationDawn::ScopedAccess>
SharedImageRepresentationDawn::BeginScopedAccess(
    WGPUTextureUsage usage,
    AllowUnclearedAccess allow_uncleared) {
  if (allow_uncleared != AllowUnclearedAccess::kYes && !IsCleared()) {
    LOG(ERROR) << "Attempt to access an uninitialized ShardImage";
    return nullptr;
  }

  WGPUTexture texture = BeginAccess(usage);
  if (!texture)
    return nullptr;
  return std::make_unique<ScopedAccess>(
      util::PassKey<SharedImageRepresentationDawn>(), this, texture);
}

}  // namespace gpu
