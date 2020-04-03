// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host_single_process.h"

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

namespace ui {

WaylandBufferManagerHostSingleProcess::WaylandBufferManagerHostSingleProcess() =
    default;

WaylandBufferManagerHostSingleProcess::
    ~WaylandBufferManagerHostSingleProcess() = default;

void WaylandBufferManagerHostSingleProcess::
    SetWaylandBufferManagerGpuSingleProcess(
        WaylandBufferManagerGpu* manager_gpu) {
  single_proc_manager_gpu_ = manager_gpu;
}

void WaylandBufferManagerHostSingleProcess::OnSubmission(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::SwapResult& swap_result) {
  DCHECK(single_proc_manager_gpu_);
  single_proc_manager_gpu_->OnBufferSubmitted(widget, buffer_id, swap_result);
}

void WaylandBufferManagerHostSingleProcess::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(single_proc_manager_gpu_);
  single_proc_manager_gpu_->OnBufferPresented(widget, buffer_id, feedback);
}

void WaylandBufferManagerHostSingleProcess::OnError(std::string error_message) {
  LOG(FATAL) << error_message;
}

}  // namespace ui
