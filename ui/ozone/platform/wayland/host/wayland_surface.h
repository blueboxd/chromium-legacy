// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_

#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"

namespace ui {

class WaylandWindow;

// Wrapper of a wl_surface, owned by a WaylandWindow or a WlSubsurface.
class WaylandSurface {
 public:
  WaylandSurface();
  WaylandSurface(const WaylandSurface&) = delete;
  WaylandSurface& operator=(const WaylandSurface&) = delete;
  ~WaylandSurface();

  WaylandWindow* root_window() const { return root_window_; }
  wl_surface* surface() const { return surface_.get(); }
  int32_t buffer_scale() const { return buffer_scale_; }

  // gfx::AcceleratedWidget identifies a wl_surface or a ui::WaylandWindow. Note
  // that GetWidget() and GetRootWidget() do not necessarily return the same
  // result.
  gfx::AcceleratedWidget GetWidget() const;
  gfx::AcceleratedWidget GetRootWidget() const;

 private:
  WaylandWindow* root_window_ = nullptr;
  wl::Object<wl_surface> surface_;
  // Wayland's scale factor for the output that this window currently belongs
  // to.
  int32_t buffer_scale_ = 1;
  friend class WaylandWindow;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SURFACE_H_
