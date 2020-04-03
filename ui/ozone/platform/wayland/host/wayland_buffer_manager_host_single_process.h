// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_SINGLE_PROCESS_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_SINGLE_PROCESS_H_

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

namespace ui {

class WaylandBufferManagerGpu;

// Same as WaylandBufferManagerHostImpl, but that uses direct connection with
// the WaylandBufferManagerGpu if mojo is not available.
class WaylandBufferManagerHostSingleProcess : public WaylandBufferManagerHost {
 public:
  WaylandBufferManagerHostSingleProcess();
  ~WaylandBufferManagerHostSingleProcess() override;
  WaylandBufferManagerHostSingleProcess(
      const WaylandBufferManagerHostSingleProcess& host) = delete;
  WaylandBufferManagerHostSingleProcess& operator=(
      const WaylandBufferManagerHostSingleProcess& host) = delete;

  // Sets a pointer to WaylandBufferManagerGpu, which is an instance of
  // WaylandBufferManagerGpuSingleProcess.
  void SetWaylandBufferManagerGpuSingleProcess(
      WaylandBufferManagerGpu* manager_gpu);

 private:
  // WaylandBufferManagerHost overrides:
  void OnSubmission(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::SwapResult& swap_result) override;
  void OnPresentation(gfx::AcceleratedWidget widget,
                      uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback) override;
  void OnError(std::string error_message) override;

  WaylandBufferManagerGpu* single_proc_manager_gpu_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_SINGLE_PROCESS_H_
