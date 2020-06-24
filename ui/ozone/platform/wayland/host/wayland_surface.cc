// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_surface.h"

#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

WaylandSurface::WaylandSurface() = default;
WaylandSurface::~WaylandSurface() = default;

gfx::AcceleratedWidget WaylandSurface::GetWidget() const {
  if (!surface_)
    return gfx::kNullAcceleratedWidget;
  return surface_.id();
}

gfx::AcceleratedWidget WaylandSurface::GetRootWidget() const {
  return root_window_->GetWidget();
}

}  // namespace ui
