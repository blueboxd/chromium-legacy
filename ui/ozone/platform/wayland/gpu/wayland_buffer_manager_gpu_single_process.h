// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_SINGLE_PROCESS_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_SINGLE_PROCESS_H_

#include <memory>

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

namespace ui {

class WaylandBufferManagerHost;

// Same as WaylandBufferManagerGpuImpl, but uses direct connection with the
// WaylandBufferManagerHost if mojo is not available.
class WaylandBufferManagerGpuSingleProcess : public WaylandBufferManagerGpu {
 public:
  explicit WaylandBufferManagerGpuSingleProcess(
      WaylandBufferManagerHost* single_proc_host);
  ~WaylandBufferManagerGpuSingleProcess() override;
  WaylandBufferManagerGpuSingleProcess(
      const WaylandBufferManagerGpuSingleProcess& manager) = delete;
  WaylandBufferManagerGpuSingleProcess& operator=(
      const WaylandBufferManagerGpuSingleProcess& manager) = delete;

  // WaylandBufferManagerGpu overrides:
  void CreateDmabufBasedBuffer(base::ScopedFD dmabuf_fd,
                               gfx::Size size,
                               const std::vector<uint32_t>& strides,
                               const std::vector<uint32_t>& offsets,
                               const std::vector<uint64_t>& modifiers,
                               uint32_t current_format,
                               uint32_t planes_count,
                               uint32_t buffer_id) override;
  void CreateShmBasedBuffer(base::ScopedFD shm_fd,
                            size_t length,
                            gfx::Size size,
                            uint32_t buffer_id) override;
  void CommitBuffer(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::Rect& damage_region) override;
  void DestroyBuffer(gfx::AcceleratedWidget widget,
                     uint32_t buffer_id) override;

 private:
  WaylandBufferManagerHost* const single_proc_host_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_SINGLE_PROCESS_H_
