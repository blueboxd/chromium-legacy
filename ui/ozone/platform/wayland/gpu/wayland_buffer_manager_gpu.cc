// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

#include <utility>

#include "base/bind.h"
#include "base/message_loop/message_loop_current.h"
#include "base/process/process.h"
#include "mojo/public/cpp/bindings/associated_interface_ptr.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"

namespace ui {

WaylandBufferManagerGpu::WaylandBufferManagerGpu()
    : associated_binding_(this) {}

WaylandBufferManagerGpu::~WaylandBufferManagerGpu() = default;

void WaylandBufferManagerGpu::SetWaylandBufferManagerHost(
    BufferManagerHostPtr buffer_manager_host_ptr) {
  // This is an IO child thread meant for IPC. Bind interface in this thread and
  // do all the mojo calls on the same thread.
  BindHostInterface(std::move(buffer_manager_host_ptr));

  io_thread_runner_ = base::ThreadTaskRunnerHandle::Get();
}

void WaylandBufferManagerGpu::ResetGbmDevice() {
#if defined(WAYLAND_GBM)
  gbm_device_.reset();
#else
  NOTREACHED();
#endif
}

void WaylandBufferManagerGpu::OnSubmission(gfx::AcceleratedWidget widget,
                                           uint32_t buffer_id,
                                           gfx::SwapResult swap_result) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = GetSurface(widget);
  // There can be a race between destruction and submitting the last frames. The
  // surface can be destroyed by the time the host receives a request to destroy
  // a buffer, and is able to call the OnSubmission for that specific buffer.
  if (surface) {
    // As long as mojo calls rerouted to the IO child thread, we have to reroute
    // them back to the same thread, where the original commit buffer call came
    // from.
    commit_thread_runner_->PostTask(
        FROM_HERE,
        base::Bind(&WaylandSurfaceGpu::OnSubmission, base::Unretained(surface),
                   buffer_id, swap_result));
  }
}

void WaylandBufferManagerGpu::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = GetSurface(widget);
  // There can be a race between destruction and presenting the last frames. The
  // surface can be destroyed by the time the host receives a request to destroy
  // a buffer, and is able to call the OnPresentation for that specific buffer.
  if (surface) {
    // As long as mojo calls rerouted to the IO child thread, we have to reroute
    // them back to the same thread, where the original commit buffer call came
    // from.
    commit_thread_runner_->PostTask(
        FROM_HERE, base::Bind(&WaylandSurfaceGpu::OnPresentation,
                              base::Unretained(surface), buffer_id, feedback));
  }
}

void WaylandBufferManagerGpu::RegisterSurface(gfx::AcceleratedWidget widget,
                                              WaylandSurfaceGpu* surface) {
  widget_to_surface_map_.emplace(widget, surface);
}

void WaylandBufferManagerGpu::UnregisterSurface(gfx::AcceleratedWidget widget) {
  widget_to_surface_map_.erase(widget);
}

WaylandSurfaceGpu* WaylandBufferManagerGpu::GetSurface(
    gfx::AcceleratedWidget widget) const {
  WaylandSurfaceGpu* surface = nullptr;
  auto it = widget_to_surface_map_.find(widget);
  if (it != widget_to_surface_map_.end())
    surface = it->second;
  return surface;
}

void WaylandBufferManagerGpu::CreateDmabufBasedBuffer(
    gfx::AcceleratedWidget widget,
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
      base::BindOnce(&WaylandBufferManagerGpu::CreateDmabufBasedBufferInternal,
                     base::Unretained(this), widget, std::move(dmabuf_fd),
                     std::move(size), std::move(strides), std::move(offsets),
                     std::move(modifiers), current_format, planes_count,
                     buffer_id));
}

void WaylandBufferManagerGpu::CreateShmBasedBuffer(
    gfx::AcceleratedWidget widget,
    base::ScopedFD shm_fd,
    size_t length,
    gfx::Size size,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_);

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::CreateShmBasedBufferInternal,
                     base::Unretained(this), widget, std::move(shm_fd), length,
                     std::move(size), buffer_id));
}

void WaylandBufferManagerGpu::CommitBuffer(gfx::AcceleratedWidget widget,
                                           uint32_t buffer_id,
                                           const gfx::Rect& damage_region) {
  DCHECK(io_thread_runner_);

  if (!commit_thread_runner_)
    commit_thread_runner_ = base::ThreadTaskRunnerHandle::Get();

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::CommitBufferInternal,
                     base::Unretained(this), widget, buffer_id, damage_region));
}

void WaylandBufferManagerGpu::DestroyBuffer(gfx::AcceleratedWidget widget,
                                            uint32_t buffer_id) {
  DCHECK(io_thread_runner_);

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WaylandBufferManagerGpu::DestroyBufferInternal,
                                base::Unretained(this), widget, buffer_id));
}

void WaylandBufferManagerGpu::AddBindingWaylandBufferManagerGpu(
    ozone::mojom::WaylandBufferManagerGpuRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void WaylandBufferManagerGpu::CreateDmabufBasedBufferInternal(
    gfx::AcceleratedWidget widget,
    base::ScopedFD dmabuf_fd,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(buffer_manager_host_ptr_);
  buffer_manager_host_ptr_->CreateDmabufBasedBuffer(
      widget,
      mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(dmabuf_fd))),
      size, strides, offsets, modifiers, current_format, planes_count,
      buffer_id);
}

void WaylandBufferManagerGpu::CreateShmBasedBufferInternal(
    gfx::AcceleratedWidget widget,
    base::ScopedFD shm_fd,
    size_t length,
    gfx::Size size,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(buffer_manager_host_ptr_);
  buffer_manager_host_ptr_->CreateShmBasedBuffer(
      widget, mojo::WrapPlatformHandle(mojo::PlatformHandle(std::move(shm_fd))),
      length, size, buffer_id);
}

void WaylandBufferManagerGpu::CommitBufferInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::Rect& damage_region) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(buffer_manager_host_ptr_);

  buffer_manager_host_ptr_->CommitBuffer(widget, buffer_id, damage_region);
}

void WaylandBufferManagerGpu::DestroyBufferInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK(buffer_manager_host_ptr_);

  buffer_manager_host_ptr_->DestroyBuffer(widget, buffer_id);
}

void WaylandBufferManagerGpu::BindHostInterface(
    BufferManagerHostPtr buffer_manager_host_ptr) {
  buffer_manager_host_ptr_.Bind(buffer_manager_host_ptr.PassInterface());

  // Setup associated interface.
  ozone::mojom::WaylandBufferManagerGpuAssociatedPtrInfo client_ptr_info;
  auto request = MakeRequest(&client_ptr_info);
  DCHECK(buffer_manager_host_ptr_);
  buffer_manager_host_ptr_->SetWaylandBufferManagerGpuPtr(
      std::move(client_ptr_info));
  associated_binding_.Bind(std::move(request));
}

}  // namespace ui
