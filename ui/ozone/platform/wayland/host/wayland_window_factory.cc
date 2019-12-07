// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <memory>

#include "ui/ozone/platform/wayland/host/wayland_surface.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

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
      // TODO(msisov): Add WaylandPopup.
      window.reset(new WaylandWindow(delegate, connection));
      break;
    case PlatformWindowType::kTooltip:
      // TODO(msisov): Add WaylandSubsurface.
      window.reset(new WaylandWindow(delegate, connection));
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