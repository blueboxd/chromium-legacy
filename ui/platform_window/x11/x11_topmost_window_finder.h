// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_TOPMOST_WINDOW_FINDER_H_
#define UI_PLATFORM_WINDOW_X11_X11_TOPMOST_WINDOW_FINDER_H_

#include <set>

#include "base/macros.h"
#include "ui/base/x/x11_topmost_window_finder.h"
#include "ui/base/x/x11_util.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/x11.h"
#include "ui/platform_window/x11/x11_window_export.h"

namespace ui {

class X11Window;

// Utility class for finding the topmost window at a given screen position.
class X11_WINDOW_EXPORT X11TopmostWindowFinder
    : public ui::EnumerateWindowsDelegate,
      public ui::XTopmostWindowFinder {
 public:
  X11TopmostWindowFinder();
  ~X11TopmostWindowFinder() override;

  // Returns the topmost window at |screen_loc_in_pixels|, ignoring the windows
  // in |ignore|. Returns null widget if the topmost window at
  // |screen_loc_in_pixels| does not belong to Chrome.
  XID FindLocalProcessWindowAt(const gfx::Point& screen_loc_in_pixels,
                               const std::set<gfx::AcceleratedWidget>& ignore);

  // Returns the topmost window at |screen_loc_in_pixels|.
  XID FindWindowAt(const gfx::Point& screen_loc_in_pixels) override;

 private:
  // ui::EnumerateWindowsDelegate:
  bool ShouldStopIterating(XID xid) override;

  // Returns true if |window| does not not belong to |ignore|, is visible and
  // contains |screen_loc_|.
  bool ShouldStopIteratingAtLocalProcessWindow(ui::X11Window* window);

  gfx::Point screen_loc_in_pixels_;
  std::set<gfx::AcceleratedWidget> ignore_;
  XID toplevel_ = x11::None;

  DISALLOW_COPY_AND_ASSIGN(X11TopmostWindowFinder);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_TOPMOST_WINDOW_FINDER_H_
