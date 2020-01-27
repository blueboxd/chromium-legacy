// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"

#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window_manager.h"

namespace ui {

namespace {

gfx::Rect AdjustSubsurfaceBounds(const gfx::Rect& bounds_px,
                                 const gfx::Rect& parent_bounds_px,
                                 int32_t ui_scale,
                                 int32_t buffer_scale) {
  const auto parent_bounds_dip =
      gfx::ScaleToRoundedRect(parent_bounds_px, 1.0 / ui_scale);
  auto new_bounds_dip =
      wl::TranslateBoundsToParentCoordinates(bounds_px, parent_bounds_dip);
  return gfx::ScaleToRoundedRect(new_bounds_dip, ui_scale / buffer_scale);
}

}  // namespace

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

void WaylandSubsurface::SetBounds(const gfx::Rect& bounds) {
  auto old_bounds = GetBounds();
  WaylandWindow::SetBounds(bounds);

  if (old_bounds == bounds || !parent_window())
    return;

  // Translate location from screen to surface coordinates.
  auto bounds_px = AdjustSubsurfaceBounds(
      GetBounds(), parent_window()->GetBounds(), ui_scale(), buffer_scale());
  wl_subsurface_set_position(subsurface_.get(), bounds_px.x() / buffer_scale(),
                             bounds_px.y() / buffer_scale());
  wl_surface_commit(surface());
  connection()->ScheduleFlush();
}

void WaylandSubsurface::CreateSubsurface() {
  auto* parent = parent_window();
  if (!parent) {
    // wl_subsurface can be used for several purposes: tooltips and drag arrow
    // windows. If we are in a drag process, use the entered window. Otherwise,
    // it must be a tooltip.
    if (connection()->IsDragInProgress()) {
      parent = connection()->wayland_data_device()->entered_window();
      set_parent_window(parent);
    } else {
      // If Aura does not not provide a reference parent window, needed by
      // Wayland, we get the current focused window to place and show the
      // tooltips.
      parent =
          connection()->wayland_window_manager()->GetCurrentFocusedWindow();
    }
  }

  // Tooltip and drag arrow creation is an async operation. By the time Aura
  // actually creates them, it is possible that the user has already moved the
  // mouse/pointer out of the window that triggered the tooltip, or user is no
  // longer in a drag/drop process. In this case, parent is NULL.
  if (!parent)
    return;

  wl_subcompositor* subcompositor = connection()->subcompositor();
  DCHECK(subcompositor);
  subsurface_.reset(wl_subcompositor_get_subsurface(subcompositor, surface(),
                                                    parent->surface()));

  // Chromium positions tooltip windows in screen coordinates, but Wayland
  // requires them to be in local surface coordinates a.k.a relative to parent
  // window.
  auto bounds_px = AdjustSubsurfaceBounds(GetBounds(), parent->GetBounds(),
                                          ui_scale(), buffer_scale());

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
  // window or a window that entered drag whenever the subsurface is created.
  if (properties.parent_widget == gfx::kNullAcceleratedWidget) {
    DCHECK(!parent_window());
    return true;
  }
  set_parent_window(GetParentWindow(properties.parent_widget));
  return true;
}

}  // namespace ui
