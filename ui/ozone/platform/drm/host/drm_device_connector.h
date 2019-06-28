// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_CONNECTOR_H_
#define UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_CONNECTOR_H_

#include <string>

#include "base/single_thread_task_runner.h"
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/interfaces/device_cursor.mojom.h"
#include "ui/ozone/public/interfaces/drm_device.mojom.h"

namespace service_manager {
class Connector;
}

namespace ui {
class HostDrmDevice;

// DrmDeviceConnector sets up mojo pipes connecting the Viz host to the DRM
// service. It operates in two modes: running on the I/O thread when invoked
// from content and running on the VizHost main thread when operating with a
// service_manager.
class DrmDeviceConnector : public GpuPlatformSupportHost {
 public:
  DrmDeviceConnector(service_manager::Connector* connector,
                     const std::string& service_name,
                     scoped_refptr<HostDrmDevice> host_drm_device);
  ~DrmDeviceConnector() override;

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

  // BindInterface arranges for the drm_device_ptr to be connected.
  void BindInterfaceDrmDevice(
      ui::ozone::mojom::DrmDevicePtr* drm_device_ptr) const;

  // BindableNow returns true if this DrmDeviceConnector is capable of binding a
  // mojo endpoint for the DrmDevice service.
  bool BindableNow() const { return !!connector_; }

 private:
  bool am_running_in_ws_mode() const { return !!ws_runner_; }

  // This will be present if the Viz host has a service manager.
  service_manager::Connector* const connector_;

  // Name of the service that provides DRM mojo interfaces.
  const std::string service_name_;

  // This will be used if we are operating under content/gpu without a service
  // manager.
  GpuHostBindInterfaceCallback binder_callback_;

  // The host_id from the last call to OnGpuServiceLaunched.
  int host_id_ = 0;

  const scoped_refptr<HostDrmDevice> host_drm_device_;
  scoped_refptr<base::SingleThreadTaskRunner> ws_runner_;

  DISALLOW_COPY_AND_ASSIGN(DrmDeviceConnector);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRM_HOST_DRM_DEVICE_CONNECTOR_H_
