// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/shell_object_factory.h"

#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/xdg_popup_wrapper_impl.h"
#include "ui/ozone/platform/wayland/host/xdg_surface_wrapper_impl.h"

namespace ui {

ShellObjectFactory::ShellObjectFactory() = default;
ShellObjectFactory::~ShellObjectFactory() = default;

std::unique_ptr<ShellSurfaceWrapper>
ShellObjectFactory::CreateShellSurfaceWrapper(WaylandConnection* connection,
                                              WaylandWindow* wayland_window) {
  if (connection->shell() || connection->shell_v6()) {
    auto surface =
        std::make_unique<XDGSurfaceWrapperImpl>(wayland_window, connection);
    return surface->Initialize(true /* with_top_level */) ? std::move(surface)
                                                          : nullptr;
  }
  LOG(WARNING) << "Shell protocol is not available.";
  return nullptr;
}

std::unique_ptr<ShellPopupWrapper> ShellObjectFactory::CreateShellPopupWrapper(
    WaylandConnection* connection,
    WaylandWindow* wayland_window,
    const gfx::Rect& bounds) {
  if (connection->shell() || connection->shell_v6()) {
    auto surface =
        std::make_unique<XDGSurfaceWrapperImpl>(wayland_window, connection);
    if (!surface->Initialize(false /* with_top_level */))
      return nullptr;

    auto popup = std::make_unique<XDGPopupWrapperImpl>(std::move(surface),
                                                       wayland_window);
    return popup->Initialize(connection, bounds) ? std::move(popup) : nullptr;
  }
  LOG(WARNING) << "Shell protocol is not available.";
  return nullptr;
}

}  // namespace ui