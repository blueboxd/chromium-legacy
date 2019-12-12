// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"

#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {

WaylandSubsurface::WaylandSubsurface(PlatformWindowDelegate* delegate,
                                     WaylandConnection* connection)
    : WaylandWindow(delegate, connection) {}

WaylandSubsurface::~WaylandSubsurface() = default;

void WaylandSubsurface::Show(bool inactive) {
  if (subsurface_)
    return;

  CreateSubsurface();
  UpdateBufferScale(false);
}

void WaylandSubsurface::Hide() {
  if (!subsurface_)
    return;

  subsurface_.reset();

  // Detach buffer from surface in order to completely shutdown menus and
  // tooltips, and release resources.
  connection()->buffer_manager_host()->ResetSurfaceContents(GetWidget());
}

bool WaylandSubsurface::IsVisible() const {
  return !!subsurface_;
}

void WaylandSubsurface::CreateSubsurface() {
  auto* parent = parent_window();
  if (!parent) {
    // If Aura does not not provide a reference parent window, needed by
    // Wayland, we get the current focused window to place and show the
    // tooltips.
    parent = connection()->wayland_window_manager()->GetCurrentFocusedWindow();
  }

  // Tooltip creation is an async operation. By the time Aura actually creates
  // the tooltip, it is possible that the user has already moved the
  // mouse/pointer out of the window that triggered the tooptip. In this case,
  // parent is NULL.
  if (!parent)
    return;

  wl_subcompositor* subcompositor = connection()->subcompositor();
  DCHECK(subcompositor);
  subsurface_.reset(wl_subcompositor_get_subsurface(subcompositor, surface(),
                                                    parent->surface()));

  // Chromium positions tooltip windows in screen coordinates, but Wayland
  // requires them to be in local surface coordinates a.k.a relative to parent
  // window.
  const auto parent_bounds_dip =
      gfx::ScaleToRoundedRect(parent->GetBounds(), 1.0 / ui_scale());
  auto new_bounds_dip =
      wl::TranslateBoundsToParentCoordinates(GetBounds(), parent_bounds_dip);
  auto bounds_px =
      gfx::ScaleToRoundedRect(new_bounds_dip, ui_scale() / buffer_scale());

  DCHECK(subsurface_);
  // Convert position to DIP.
  wl_subsurface_set_position(subsurface_.get(), bounds_px.x() / buffer_scale(),
                             bounds_px.y() / buffer_scale());
  wl_subsurface_set_desync(subsurface_.get());
  wl_surface_commit(parent->surface());
  connection()->ScheduleFlush();
}

bool WaylandSubsurface::OnInitialize(PlatformWindowInitProperties properties) {
  // If we do not have parent window provided, we must always use a focused
  // window whenever the subsurface is created.
  if (properties.parent_widget == gfx::kNullAcceleratedWidget) {
    DCHECK(!parent_window());
    return true;
  }
  set_parent_window(GetParentWindow(properties.parent_widget));
  return true;
}

}  // namespace ui
