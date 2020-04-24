// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
#define UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_

#include "base/macros.h"
#include "ui/platform_window/x11/x11_window.h"

namespace ui {

class PlatformWindowDelegate;

// PlatformWindow implementation for X11 Ozone. PlatformEvents are ui::Events.
class X11WindowOzone : public X11Window {
 public:
  explicit X11WindowOzone(PlatformWindowDelegate* delegate);
  ~X11WindowOzone() override;

  // Overridden from PlatformWindow:
  void SetCursor(PlatformCursor cursor) override;

  DISALLOW_COPY_AND_ASSIGN(X11WindowOzone);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_X11_X11_WINDOW_OZONE_H_
