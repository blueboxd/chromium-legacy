// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu_single_process.h"

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

namespace ui {

WaylandBufferManagerGpuSingleProcess::WaylandBufferManagerGpuSingleProcess(
    WaylandBufferManagerHost* single_proc_host)
    : single_proc_host_(single_proc_host) {
  DCHECK(single_proc_host_);
}

WaylandBufferManagerGpuSingleProcess::~WaylandBufferManagerGpuSingleProcess() =
    default;

void WaylandBufferManagerGpuSingleProcess::CreateDmabufBasedBuffer(
    base::ScopedFD dmabuf_fd,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  single_proc_host_->CreateBufferDmabuf(std::move(dmabuf_fd), size, strides,
                                        offsets, modifiers, current_format,
                                        planes_count, buffer_id);
}

void WaylandBufferManagerGpuSingleProcess::CreateShmBasedBuffer(
    base::ScopedFD shm_fd,
    size_t length,
    gfx::Size size,
    uint32_t buffer_id) {
  single_proc_host_->CreateBufferShm(std::move(shm_fd), length, size,
                                     buffer_id);
}

void WaylandBufferManagerGpuSingleProcess::CommitBuffer(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::Rect& damage_region) {
  single_proc_host_->CommitBufferWithId(widget, buffer_id, damage_region);
}

void WaylandBufferManagerGpuSingleProcess::DestroyBuffer(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  single_proc_host_->DestroyBufferWithId(widget, buffer_id);
}

}  // namespace ui
