// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_topmost_window_finder.h"

#include <stddef.h>

#include <vector>

#include "ui/gfx/native_widget_types.h"
#include "ui/platform_window/x11/x11_window.h"
#include "ui/platform_window/x11/x11_window_manager.h"

namespace ui {

X11TopmostWindowFinder::X11TopmostWindowFinder() = default;

X11TopmostWindowFinder::~X11TopmostWindowFinder() = default;

x11::Window X11TopmostWindowFinder::FindLocalProcessWindowAt(
    const gfx::Point& screen_loc_in_pixels,
    const std::set<gfx::AcceleratedWidget>& ignore) {
  screen_loc_in_pixels_ = screen_loc_in_pixels;
  ignore_ = ignore;

  std::vector<X11Window*> local_process_windows =
      X11WindowManager::GetInstance()->GetAllOpenWindows();
  if (std::none_of(local_process_windows.cbegin(), local_process_windows.cend(),
                   [this](auto* window) {
                     return ShouldStopIteratingAtLocalProcessWindow(window);
                   })) {
    return x11::Window::None;
  }

  EnumerateTopLevelWindows(this);
  return toplevel_;
}

x11::Window X11TopmostWindowFinder::FindWindowAt(
    const gfx::Point& screen_loc_in_pixels) {
  screen_loc_in_pixels_ = screen_loc_in_pixels;
  EnumerateTopLevelWindows(this);
  return toplevel_;
}

bool X11TopmostWindowFinder::ShouldStopIterating(x11::Window xwindow) {
  if (!IsWindowVisible(xwindow))
    return false;

  auto* window = X11WindowManager::GetInstance()->GetWindow(
      static_cast<gfx::AcceleratedWidget>(xwindow));
  if (window) {
    if (ShouldStopIteratingAtLocalProcessWindow(window)) {
      toplevel_ = xwindow;
      return true;
    }
    return false;
  }

  if (WindowContainsPoint(xwindow, screen_loc_in_pixels_)) {
    toplevel_ = xwindow;
    return true;
  }
  return false;
}

bool X11TopmostWindowFinder::ShouldStopIteratingAtLocalProcessWindow(
    X11Window* window) {
  if (ignore_.find(window->GetWidget()) != ignore_.end())
    return false;

  // Currently |window|->IsVisible() always returns true.
  // TODO(pkotwicz): Fix this. crbug.com/353038
  if (!window->IsVisible())
    return false;

  gfx::Rect window_bounds = window->GetOuterBounds();
  if (!window_bounds.Contains(screen_loc_in_pixels_))
    return false;

  gfx::Point window_point(screen_loc_in_pixels_);
  window_point.Offset(-window_bounds.origin().x(), -window_bounds.origin().y());
  return window->ContainsPointInXRegion(window_point);
}

}  // namespace ui
