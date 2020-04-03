// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_IMPL_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_IMPL_H_

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/public/mojom/wayland/wayland_buffer_manager.mojom.h"

namespace ui {

// Implementation of WaylandBufferManagerHost that communicates with
// WaylandBufferManagerGpu through mojo. Can be used in both single and multi
// process mode.
class WaylandBufferManagerHostImpl
    : public ozone::mojom::WaylandBufferManagerHost,
      public WaylandBufferManagerHost {
 public:
  WaylandBufferManagerHostImpl();
  ~WaylandBufferManagerHostImpl() override;
  WaylandBufferManagerHostImpl(const WaylandBufferManagerHostImpl& host) =
      delete;
  WaylandBufferManagerHostImpl& operator=(
      const WaylandBufferManagerHostImpl& host) = delete;

  void SetTerminateGpuCallback(
      base::OnceCallback<void(std::string)> terminate_gpu_cb);

  // Returns bound pointer to own mojo interface.
  mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> BindInterface();

  // Unbinds the interface and clears the state of the |buffer_manager_|. Used
  // only when the GPU channel, which uses the mojo pipe to this interface, is
  // destroyed.
  void OnChannelDestroyed();

  // ozone::mojom::WaylandBufferManagerHost overrides:
  //
  // These overridden methods below are invoked by the GPU when hardware
  // accelerated rendering is used.
  void SetWaylandBufferManagerGpu(
      mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
          buffer_manager_gpu_associated) override;
  //
  // Check comments in the
  // ui/ozone/public/mojom/wayland/wayland_connection.mojom.
  void CreateDmabufBasedBuffer(mojo::PlatformHandle dmabuf_fd,
                               const gfx::Size& size,
                               const std::vector<uint32_t>& strides,
                               const std::vector<uint32_t>& offsets,
                               const std::vector<uint64_t>& modifiers,
                               uint32_t format,
                               uint32_t planes_count,
                               uint32_t buffer_id) override;
  void CreateShmBasedBuffer(mojo::PlatformHandle shm_fd,
                            uint64_t length,
                            const gfx::Size& size,
                            uint32_t buffer_id) override;
  void DestroyBuffer(gfx::AcceleratedWidget widget,
                     uint32_t buffer_id) override;
  void CommitBuffer(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::Rect& damage_region) override;

 private:
  // WaylandBufferManagerHost overrides:
  void OnSubmission(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::SwapResult& swap_result) override;
  void OnPresentation(gfx::AcceleratedWidget widget,
                      uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback) override;
  void OnError(std::string error_message) override;

  mojo::AssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
      buffer_manager_gpu_associated_;
  mojo::Receiver<ozone::mojom::WaylandBufferManagerHost> receiver_;

  // A callback, which is used to terminate a GPU process in case of invalid
  // data sent by the GPU to the browser process.
  base::OnceCallback<void(std::string)> terminate_gpu_cb_;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_IMPL_H_
