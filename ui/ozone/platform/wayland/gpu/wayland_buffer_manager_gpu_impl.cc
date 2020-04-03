// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop_current.h"
#include "base/process/process.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"

namespace ui {

WaylandBufferManagerGpuImpl::WaylandBufferManagerGpuImpl()
    : receiver_(this), associated_receiver_(this) {}
WaylandBufferManagerGpuImpl::~WaylandBufferManagerGpuImpl() = default;

void WaylandBufferManagerGpuImpl::Initialize(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host,
    const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
        buffer_formats_with_modifiers,
    bool supports_dma_buf) {
  StoreBufferFormatsWithModifiers(buffer_formats_with_modifiers,
                                  supports_dma_buf);

  BindHostInterface(std::move(remote_host));

  io_thread_runner_ = base::ThreadTaskRunnerHandle::Get();
}

void WaylandBufferManagerGpuImpl::OnSubmission(gfx::AcceleratedWidget widget,
                                               uint32_t buffer_id,
                                               gfx::SwapResult swap_result) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  // Return back to the same thread where the commit request came from.
  commit_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpuImpl::OnBufferSubmitted,
                     base::Unretained(this), widget, buffer_id, swap_result));
}

void WaylandBufferManagerGpuImpl::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  // Return back to the same thread where the commit request came from.
  commit_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpuImpl::OnBufferPresented,
                     base::Unretained(this), widget, buffer_id, feedback));
}

void WaylandBufferManagerGpuImpl::CreateDmabufBasedBuffer(
    base::ScopedFD dmabuf_fd,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_);

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WaylandBufferManagerGpuImpl::CreateDmabufBasedBufferInternal,
          base::Unretained(this), std::move(dmabuf_fd), std::move(size),
          std::move(strides), std::move(offsets), std::move(modifiers),
          current_format, planes_count, buffer_id));
}

void WaylandBufferManagerGpuImpl::CreateShmBasedBuffer(base::ScopedFD shm_fd,
                                                       size_t length,
                                                       gfx::Size size,
                                                       uint32_t buffer_id) {
  DCHECK(io_thread_runner_);

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpuImpl::CreateShmBasedBufferInternal,
                     base::Unretained(this), std::move(shm_fd), length,
                     std::move(size), buffer_id));
}

void WaylandBufferManagerGpuImpl::CommitBuffer(gfx::AcceleratedWidget widget,
                                               uint32_t buffer_id,
                                               const gfx::Rect& damage_region) {
  DCHECK(io_thread_runner_);

  if (!commit_thread_runner_)
    commit_thread_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpuImpl::CommitBufferInternal,
                     base::Unretained(this), widget, buffer_id, damage_region));
}

void WaylandBufferManagerGpuImpl::DestroyBuffer(gfx::AcceleratedWidget widget,
                                                uint32_t buffer_id) {
  DCHECK(io_thread_runner_);

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpuImpl::DestroyBufferInternal,
                     base::Unretained(this), widget, buffer_id));
}

void WaylandBufferManagerGpuImpl::AddBindingWaylandBufferManagerGpu(
    mojo::PendingReceiver<ozone::mojom::WaylandBufferManagerGpu> receiver) {
  receiver_.Bind(std::move(receiver));
}

void WaylandBufferManagerGpuImpl::CreateDmabufBasedBufferInternal(
    base::ScopedFD dmabuf_fd,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(remote_host_);
  remote_host_->CreateDmabufBasedBuffer(
      mojo::PlatformHandle(std::move(dmabuf_fd)), size, strides, offsets,
      modifiers, current_format, planes_count, buffer_id);
}

void WaylandBufferManagerGpuImpl::CreateShmBasedBufferInternal(
    base::ScopedFD shm_fd,
    size_t length,
    gfx::Size size,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(remote_host_);
  remote_host_->CreateShmBasedBuffer(mojo::PlatformHandle(std::move(shm_fd)),
                                     length, size, buffer_id);
}

void WaylandBufferManagerGpuImpl::CommitBufferInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::Rect& damage_region) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(remote_host_);

  remote_host_->CommitBuffer(widget, buffer_id, damage_region);
}

void WaylandBufferManagerGpuImpl::DestroyBufferInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(remote_host_);

  remote_host_->DestroyBuffer(widget, buffer_id);
}

void WaylandBufferManagerGpuImpl::BindHostInterface(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host) {
  remote_host_.Bind(std::move(remote_host));

  // Setup associated interface.
  mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
      client_remote;
  associated_receiver_.Bind(client_remote.InitWithNewEndpointAndPassReceiver());
  DCHECK(remote_host_);
  remote_host_->SetWaylandBufferManagerGpu(std::move(client_remote));
}

}  // namespace ui