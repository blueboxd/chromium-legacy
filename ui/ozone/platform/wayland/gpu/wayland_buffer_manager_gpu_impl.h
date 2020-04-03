// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_IMPL_H_

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/public/mojom/wayland/wayland_buffer_manager.mojom.h"

namespace ui {

// Forwards calls through an associated mojo connection to WaylandBufferManager
// on the browser process side.
//
// It's guaranteed that WaylandBufferManagerGpuImpl makes mojo calls on the
// right sequence.
class WaylandBufferManagerGpuImpl
    : public ozone::mojom::WaylandBufferManagerGpu,
      public WaylandBufferManagerGpu {
 public:
  WaylandBufferManagerGpuImpl();
  ~WaylandBufferManagerGpuImpl() override;
  WaylandBufferManagerGpuImpl(const WaylandBufferManagerGpuImpl& manager) =
      delete;
  WaylandBufferManagerGpuImpl& operator=(
      const WaylandBufferManagerGpuImpl& manager) = delete;

  // ozone::mojom::WaylandBufferManagerGpu overrides:
  void Initialize(
      mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host,
      const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
          buffer_formats_with_modifiers,
      bool supports_dma_buf) override;
  void OnSubmission(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    gfx::SwapResult swap_result) override;
  void OnPresentation(gfx::AcceleratedWidget widget,
                      uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback) override;

  // Methods, which can be used when in both in-process-gpu and out of process
  // modes. These calls are forwarded to the browser process through the
  // WaylandBufferManagerHost mojo interface. See more in
  // ui/ozone/public/mojom/wayland/wayland_connection.mojom.
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

  // Adds a WaylandBufferManagerGpu binding.
  void AddBindingWaylandBufferManagerGpu(
      mojo::PendingReceiver<ozone::mojom::WaylandBufferManagerGpu> receiver);

 private:
  void CreateDmabufBasedBufferInternal(base::ScopedFD dmabuf_fd,
                                       gfx::Size size,
                                       const std::vector<uint32_t>& strides,
                                       const std::vector<uint32_t>& offsets,
                                       const std::vector<uint64_t>& modifiers,
                                       uint32_t current_format,
                                       uint32_t planes_count,
                                       uint32_t buffer_id);
  void CreateShmBasedBufferInternal(base::ScopedFD shm_fd,
                                    size_t length,
                                    gfx::Size size,
                                    uint32_t buffer_id);
  void CommitBufferInternal(gfx::AcceleratedWidget widget,
                            uint32_t buffer_id,
                            const gfx::Rect& damage_region);
  void DestroyBufferInternal(gfx::AcceleratedWidget widget, uint32_t buffer_id);

  void BindHostInterface(
      mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host);

  mojo::Receiver<ozone::mojom::WaylandBufferManagerGpu> receiver_;

  // A pointer to a WaylandBufferManagerHost object, which always lives on a
  // browser process side. It's used for a multi-process mode.
  mojo::Remote<ozone::mojom::WaylandBufferManagerHost> remote_host_;

  mojo::AssociatedReceiver<ozone::mojom::WaylandBufferManagerGpu>
      associated_receiver_;

  // This task runner can be used to pass messages back to the same thread,
  // where the commit buffer request came from. For example, swap requests come
  // from the GpuMainThread, but rerouted to the IOChildThread and then mojo
  // calls happen. However, when the manager receives mojo calls, it has to
  // reroute calls back to the same thread where the calls came from to ensure
  // correct sequence.
  scoped_refptr<base::SingleThreadTaskRunner> commit_thread_runner_;

  // A task runner, which is initialized in a multi-process mode. It is used to
  // ensure all the methods of this class are run on IOChildThread. This is
  // needed to ensure mojo calls happen on a right sequence.
  scoped_refptr<base::SingleThreadTaskRunner> io_thread_runner_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_BUFFER_MANAGER_GPU_IMPL_H_
