// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SERVER_WAYLAND_SERVER_CONTROLLER_H_
#define COMPONENTS_EXO_SERVER_WAYLAND_SERVER_CONTROLLER_H_

#include <memory>

#include "base/macros.h"

namespace exo {

namespace wayland {
class Server;
class WaylandWatcher;
}  // namespace wayland

class Display;
class FileHelper;
class WMHelper;
class NotificationSurfaceManager;
class InputMethodSurfaceManager;

class WaylandServerController {
 public:
  static std::unique_ptr<WaylandServerController> CreateForArcIfNecessary(
      std::unique_ptr<FileHelper> file_helper);

  // Creates WaylandServerController. Returns null if controller should not be
  // created.
  static std::unique_ptr<WaylandServerController> CreateIfNecessary(
      std::unique_ptr<FileHelper> file_helper,
      std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
      std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager);

  ~WaylandServerController();

  InputMethodSurfaceManager* input_method_surface_manager() {
    return input_method_surface_manager_.get();
  }

  WaylandServerController(
      std::unique_ptr<FileHelper> file_helper,
      std::unique_ptr<NotificationSurfaceManager> notification_surface_manager,
      std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager);

 private:
  std::unique_ptr<WMHelper> wm_helper_;
  std::unique_ptr<Display> display_;
  std::unique_ptr<wayland::Server> wayland_server_;
  std::unique_ptr<wayland::WaylandWatcher> wayland_watcher_;
  std::unique_ptr<NotificationSurfaceManager> notification_surface_manager_;
  std::unique_ptr<InputMethodSurfaceManager> input_method_surface_manager_;

  DISALLOW_COPY_AND_ASSIGN(WaylandServerController);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SERVER_WAYLAND_SERVER_CONTROLLER_H_
