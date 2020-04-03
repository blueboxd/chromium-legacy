// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host_impl.h"

#include "base/message_loop/message_loop_current.h"

namespace ui {

WaylandBufferManagerHostImpl::WaylandBufferManagerHostImpl()
    : receiver_(this) {}

WaylandBufferManagerHostImpl::~WaylandBufferManagerHostImpl() = default;

void WaylandBufferManagerHostImpl::SetTerminateGpuCallback(
    base::OnceCallback<void(std::string)> terminate_callback) {
  terminate_gpu_cb_ = std::move(terminate_callback);
}

mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
WaylandBufferManagerHostImpl::BindInterface() {
  DCHECK(!receiver_.is_bound());
  mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
      buffer_manager_host;
  receiver_.Bind(buffer_manager_host.InitWithNewPipeAndPassReceiver());
  return buffer_manager_host;
}

void WaylandBufferManagerHostImpl::OnChannelDestroyed() {
  buffer_manager_gpu_associated_.reset();
  receiver_.reset();

  ClearInternalState();
}

void WaylandBufferManagerHostImpl::SetWaylandBufferManagerGpu(
    mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
        buffer_manager_gpu_associated) {
  buffer_manager_gpu_associated_.Bind(std::move(buffer_manager_gpu_associated));
}

void WaylandBufferManagerHostImpl::CreateDmabufBasedBuffer(
    mojo::PlatformHandle dmabuf_fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  CreateBufferDmabuf(dmabuf_fd.TakeFD(), size, strides, offsets, modifiers,
                     format, planes_count, buffer_id);
}

void WaylandBufferManagerHostImpl::CreateShmBasedBuffer(
    mojo::PlatformHandle shm_fd,
    uint64_t length,
    const gfx::Size& size,
    uint32_t buffer_id) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  CreateBufferShm(shm_fd.TakeFD(), length, size, buffer_id);
}

void WaylandBufferManagerHostImpl::CommitBuffer(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::Rect& damage_region) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  CommitBufferWithId(widget, buffer_id, damage_region);
}

void WaylandBufferManagerHostImpl::DestroyBuffer(gfx::AcceleratedWidget widget,
                                                 uint32_t buffer_id) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());
  DestroyBufferWithId(widget, buffer_id);
}

void WaylandBufferManagerHostImpl::OnSubmission(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::SwapResult& swap_result) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  DCHECK(buffer_manager_gpu_associated_);
  buffer_manager_gpu_associated_->OnSubmission(widget, buffer_id, swap_result);
}

void WaylandBufferManagerHostImpl::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(base::MessageLoopCurrentForUI::IsSet());

  DCHECK(buffer_manager_gpu_associated_);
  buffer_manager_gpu_associated_->OnPresentation(widget, buffer_id, feedback);
}

void WaylandBufferManagerHostImpl::OnError(std::string error_message) {
  DCHECK(!error_message.empty());
  std::move(terminate_gpu_cb_).Run(std::move(error_message));
  // The GPU process' failure results in calling ::OnChannelDestroyed.
}

}  // namespace ui
