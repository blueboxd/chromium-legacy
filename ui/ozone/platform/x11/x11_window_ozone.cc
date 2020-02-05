// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_ozone.h"

#include "ui/ozone/platform/x11/x11_cursor_ozone.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

X11WindowOzone::X11WindowOzone(PlatformWindowDelegate* delegate)
    : X11Window(delegate) {}

X11WindowOzone::~X11WindowOzone() = default;

void X11WindowOzone::SetCursor(PlatformCursor cursor) {
  X11CursorOzone* cursor_ozone = static_cast<X11CursorOzone*>(cursor);
  XWindow::SetCursor(cursor_ozone->xcursor());
}

}  // namespace ui
