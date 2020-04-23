// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/window_finder.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/platform_window/x11/x11_topmost_window_finder.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host_linux.h"

namespace {

float GetDeviceScaleFactor() {
  return display::Screen::GetScreen()
      ->GetPrimaryDisplay()
      .device_scale_factor();
}

gfx::Point DIPToPixelPoint(const gfx::Point& dip_point) {
  return gfx::ScaleToFlooredPoint(dip_point, GetDeviceScaleFactor());
}

}  // anonymous namespace

gfx::NativeWindow WindowFinder::GetLocalProcessWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<gfx::NativeWindow>& ignore) {
  std::set<gfx::AcceleratedWidget> ignore_top_level;
  for (auto* const window : ignore)
    ignore_top_level.emplace(window->GetHost()->GetAcceleratedWidget());

  // The X11 server is the canonical state of what the window stacking order is.
  ui::X11TopmostWindowFinder finder;
  auto accelerated_widget = finder.FindLocalProcessWindowAt(
      DIPToPixelPoint(screen_point), ignore_top_level);
  return accelerated_widget
             ? views::DesktopWindowTreeHostLinux::GetContentWindowForWidget(
                   static_cast<gfx::AcceleratedWidget>(accelerated_widget))
             : nullptr;
}
