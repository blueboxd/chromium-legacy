// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <memory>

#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_popup.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_surface.h"

namespace ui {

// static
std::unique_ptr<WaylandWindow> WaylandWindow::Create(
    PlatformWindowDelegate* delegate,
    WaylandConnection* connection,
    PlatformWindowInitProperties properties) {
  std::unique_ptr<WaylandWindow> window;
  switch (properties.type) {
    case PlatformWindowType::kMenu:
    case PlatformWindowType::kPopup:
      // We are unable to create a popup or menu window, because they require a
      // parent window to be set. Thus, create a normal window instead then.
      if (properties.parent_widget == gfx::kNullAcceleratedWidget &&
          !connection->wayland_window_manager()->GetCurrentFocusedWindow()) {
        window.reset(new WaylandSurface(delegate, connection));
      } else if (connection->IsDragInProgress()) {
        // We are in the process of drag and requested a popup. Most probably,
        // it is an arrow window.
        window.reset(new WaylandSubsurface(delegate, connection));
      } else {
        window.reset(new WaylandPopup(delegate, connection));
      }
      break;
    case PlatformWindowType::kTooltip:
      window.reset(new WaylandSubsurface(delegate, connection));
      break;
    case PlatformWindowType::kWindow:
    case PlatformWindowType::kBubble:
    case PlatformWindowType::kDrag:
      // TODO(msisov): Figure out what kind of surface we need to create for
      // bubble and drag windows.
      window.reset(new WaylandSurface(delegate, connection));
      break;
    default:
      NOTREACHED();
      break;
  }
  return window && window->Initialize(std::move(properties)) ? std::move(window)
                                                             : nullptr;
}

}  // namespace ui