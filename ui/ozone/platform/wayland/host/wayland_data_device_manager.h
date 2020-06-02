// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_MANAGER_H_

#include <wayland-client.h>

#include <memory>

#include "base/macros.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandConnection;
class WaylandDataDevice;
class WaylandDataSource;

class WaylandDataDeviceManager {
 public:
  WaylandDataDeviceManager(wl_data_device_manager* device_manager,
                           WaylandConnection* connection);
  ~WaylandDataDeviceManager();

  WaylandDataDevice* GetDevice();
  std::unique_ptr<WaylandDataSource> CreateSource();

 private:
  wl::Object<wl_data_device_manager> device_manager_;

  WaylandConnection* const connection_;

  std::unique_ptr<WaylandDataDevice> data_device_;

  DISALLOW_COPY_AND_ASSIGN(WaylandDataDeviceManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_DATA_DEVICE_MANAGER_H_
