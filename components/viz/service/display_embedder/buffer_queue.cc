// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/buffer_queue.h"

#include <utility>

#include "base/containers/adapters.h"
#include "build/build_config.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace viz {

BufferQueue::BufferQueue(gpu::SharedImageInterface* sii,
                         gfx::BufferFormat format,
                         gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
                         gpu::SurfaceHandle surface_handle)
    : sii_(sii),
      allocated_count_(0),
      format_(format),
      gpu_memory_buffer_manager_(gpu_memory_buffer_manager),
      surface_handle_(surface_handle) {}

BufferQueue::~BufferQueue() {
  FreeAllSurfaces();
}

void BufferQueue::SetSyncTokenProvider(SyncTokenProvider* sync_token_provider) {
  DCHECK(!sync_token_provider_);
  sync_token_provider_ = sync_token_provider;
}

gpu::Mailbox BufferQueue::GetCurrentBuffer(
    gpu::SyncToken* creation_sync_token) {
  if (!current_surface_)
    current_surface_ = GetNextSurface();
  if (!current_surface_)
    return gpu::Mailbox();
  if (creation_sync_token)
    *creation_sync_token = current_surface_->creation_sync_token;
  return current_surface_->mailbox;
}

void BufferQueue::UpdateBufferDamage(const gfx::Rect& damage) {
  if (displayed_surface_)
    displayed_surface_->damage.Union(damage);
  for (auto& surface : available_surfaces_)
    surface->damage.Union(damage);
  for (auto& surface : in_flight_surfaces_) {
    if (surface)
      surface->damage.Union(damage);
  }
}

gfx::Rect BufferQueue::CurrentBufferDamage() const {
  DCHECK(current_surface_);
  return current_surface_->damage;
}

void BufferQueue::SwapBuffers(const gfx::Rect& damage) {
  UpdateBufferDamage(damage);
  if (current_surface_)
    current_surface_->damage = gfx::Rect();
  in_flight_surfaces_.push_back(std::move(current_surface_));
}

bool BufferQueue::Reshape(const gfx::Size& size,
                          const gfx::ColorSpace& color_space) {
  if (size == size_ && color_space == color_space_) {
    return false;
  }
#if !defined(OS_MACOSX)
  // TODO(ccameron): This assert is being hit on Mac try jobs. Determine if that
  // is cause for concern or if it is benign.
  // http://crbug.com/524624
  DCHECK(!current_surface_);
#endif
  size_ = size;
  color_space_ = color_space;

  FreeAllSurfaces();
  return true;
}

void BufferQueue::PageFlipComplete() {
  DCHECK(!in_flight_surfaces_.empty());
  if (in_flight_surfaces_.front()) {
    if (displayed_surface_)
      available_surfaces_.push_back(std::move(displayed_surface_));
    displayed_surface_ = std::move(in_flight_surfaces_.front());
  }

  in_flight_surfaces_.pop_front();
}

void BufferQueue::FreeAllSurfaces() {
  DCHECK(sync_token_provider_);
  const gpu::SyncToken destruction_sync_token =
      sync_token_provider_->GenSyncToken();
  FreeSurface(std::move(displayed_surface_), destruction_sync_token);
  FreeSurface(std::move(current_surface_), destruction_sync_token);

  // This is intentionally not emptied since the swap buffers acks are still
  // expected to arrive.
  for (auto& surface : in_flight_surfaces_) {
    FreeSurface(std::move(surface), destruction_sync_token);
  }

  for (auto& surface : available_surfaces_) {
    FreeSurface(std::move(surface), destruction_sync_token);
  }
  available_surfaces_.clear();
}

void BufferQueue::FreeSurface(std::unique_ptr<AllocatedSurface> surface,
                              const gpu::SyncToken& sync_token) {
  if (!surface)
    return;
  DCHECK(!surface->mailbox.IsZero());
  sii_->DestroySharedImage(sync_token, surface->mailbox);
  surface->buffer.reset();
  allocated_count_--;
}

std::unique_ptr<BufferQueue::AllocatedSurface> BufferQueue::GetNextSurface() {
  if (!available_surfaces_.empty()) {
    std::unique_ptr<AllocatedSurface> surface =
        std::move(available_surfaces_.back());
    available_surfaces_.pop_back();
    return surface;
  }

  // We don't want to allow anything more than triple buffering.
  DCHECK_LT(allocated_count_, 3U);

  // TODO(crbug.com/958670): if we can have a CreateSharedImage() that takes a
  // SurfaceHandle, we don't have to create a GpuMemoryBuffer here.
  std::unique_ptr<gfx::GpuMemoryBuffer> buffer(
      gpu_memory_buffer_manager_->CreateGpuMemoryBuffer(
          size_, format_, gfx::BufferUsage::SCANOUT, surface_handle_));
  if (!buffer) {
    LOG(ERROR) << "Failed to allocate GPU memory buffer";
    return nullptr;
  }
  buffer->SetColorSpace(color_space_);

  const gpu::Mailbox mailbox = sii_->CreateSharedImage(
      buffer.get(), gpu_memory_buffer_manager_, color_space_,
      gpu::SHARED_IMAGE_USAGE_SCANOUT |
          gpu::SHARED_IMAGE_USAGE_GLES2_FRAMEBUFFER_HINT);
  if (mailbox.IsZero()) {
    LOG(ERROR) << "Failed to create SharedImage";
    return nullptr;
  }

  allocated_count_++;
  gpu::SyncToken creation_sync_token = sii_->GenUnverifiedSyncToken();
  return std::make_unique<AllocatedSurface>(
      std::move(buffer), mailbox, creation_sync_token, gfx::Rect(size_));
}

BufferQueue::AllocatedSurface::AllocatedSurface(
    std::unique_ptr<gfx::GpuMemoryBuffer> buffer,
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& creation_sync_token,
    const gfx::Rect& rect)
    : buffer(buffer.release()),
      mailbox(mailbox),
      creation_sync_token(creation_sync_token),
      damage(rect) {}

BufferQueue::AllocatedSurface::~AllocatedSurface() {
  DCHECK(!buffer);
}

}  // namespace viz
