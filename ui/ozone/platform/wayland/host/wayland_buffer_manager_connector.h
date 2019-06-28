// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_CONNECTOR_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_CONNECTOR_H_

#include "ui/ozone/public/gpu_platform_support_host.h"

#include "ui/ozone/public/interfaces/wayland/wayland_buffer_manager.mojom.h"

namespace ui {

class WaylandBufferManagerHost;

// A connector class which instantiates a connection between
// WaylandBufferManagerGpu on the GPU side and the WaylandBufferManagerHost
// object on the browser process side.
class WaylandBufferManagerConnector : public GpuPlatformSupportHost {
 public:
  explicit WaylandBufferManagerConnector(
      WaylandBufferManagerHost* buffer_manager);
  ~WaylandBufferManagerConnector() override;

  // GpuPlatformSupportHost:
  void OnGpuProcessLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> send_runner,
      base::RepeatingCallback<void(IPC::Message*)> send_callback) override;
  void OnChannelDestroyed(int host_id) override;
  void OnMessageReceived(const IPC::Message& message) override;
  void OnGpuServiceLaunched(
      int host_id,
      scoped_refptr<base::SingleThreadTaskRunner> ui_runner,
      scoped_refptr<base::SingleThreadTaskRunner> io_runner,
      GpuHostBindInterfaceCallback binder,
      GpuHostTerminateCallback terminate_callback) override;

 private:
  void OnBufferManagerHostPtrBinded(
      ozone::mojom::WaylandBufferManagerHostPtr buffer_manager_host_ptr) const;

  void OnTerminateGpuProcess(std::string message);

  // Non-owning pointer, which is used to bind a mojo pointer to the
  // WaylandBufferManagerHost.
  WaylandBufferManagerHost* const buffer_manager_;

  GpuHostBindInterfaceCallback binder_;
  GpuHostTerminateCallback terminate_callback_;

  scoped_refptr<base::SingleThreadTaskRunner> io_runner_;

  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerConnector);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_CONNECTOR_H_
