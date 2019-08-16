// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/x11_window_event_filter.h"

#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test_x11.h"
#include "ui/base/x/x11_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/views/linux_ui/linux_ui.h"
#include "ui/views/widget/desktop_aura/desktop_window_tree_host.h"
#include "ui/views/widget/native_widget_aura.h"
#include "ui/views/widget/widget.h"

namespace views {

X11WindowEventFilter::X11WindowEventFilter(
    DesktopWindowTreeHost* window_tree_host)
    : WindowEventFilter(window_tree_host),
      xdisplay_(gfx::GetXDisplay()),
      xwindow_(window_tree_host->AsWindowTreeHost()->GetAcceleratedWidget()),
      x_root_window_(DefaultRootWindow(xdisplay_)) {}

X11WindowEventFilter::~X11WindowEventFilter() = default;

void X11WindowEventFilter::MaybeDispatchHostWindowDragMovement(
    int hittest,
    ui::MouseEvent* event) {
  if (event->IsLeftMouseButton() && event->native_event()) {
    // Get the |x_root_window_| location out of the native event.
    const gfx::Point x_root_location =
        ui::EventSystemLocationFromNative(event->native_event());
    if (DispatchHostWindowDragMovement(hittest, x_root_location))
      event->StopPropagation();
  }
}

void X11WindowEventFilter::LowerWindow() {
  XLowerWindow(xdisplay_, xwindow_);
}

bool X11WindowEventFilter::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& screen_location) {
  int direction = ui::HitTestToWmMoveResizeDirection(hittest);
  if (direction == -1)
    return false;

  ui::DoWMMoveResize(xdisplay_, x_root_window_, xwindow_, screen_location,
                     direction);
  return true;
}

}  // namespace views
