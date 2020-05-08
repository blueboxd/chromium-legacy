// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/x11/x11_window_ozone_chromeos.h"

#include "ui/events/event.h"
#include "ui/ozone/platform/x11/x11_cursor_ozone.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/x11/x11_window_manager.h"

namespace ui {

X11WindowOzone::X11WindowOzone(PlatformWindowDelegate* delegate)
    : X11Window(delegate) {}

X11WindowOzone::~X11WindowOzone() = default;

void X11WindowOzone::SetCursor(PlatformCursor cursor) {
  X11CursorOzone* cursor_ozone = static_cast<X11CursorOzone*>(cursor);
  XWindow::SetCursor(cursor_ozone->xcursor());
}

void X11WindowOzone::Initialize(PlatformWindowInitProperties properties) {
  X11Window::Initialize(std::move(properties));
}

}  // namespace ui
