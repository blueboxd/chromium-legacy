// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/win/test/scoped_screen_win.h"
#include "ui/display/win/display_info.h"
#include "ui/display/win/test/screen_util_win.h"

namespace display {
namespace win {
namespace test {

ScopedScreenWin::ScopedScreenWin() : ScreenWin(false) {
  const gfx::Rect pixel_bounds = gfx::Rect(0, 0, 1920, 1200);
  const gfx::Rect pixel_work = gfx::Rect(0, 0, 1920, 1100);
  MONITORINFOEX monitor_info =
      CreateMonitorInfo(pixel_bounds, pixel_work, L"primary");
  std::vector<DisplayInfo> display_infos;
  display_infos.push_back(
      DisplayInfo(monitor_info, 1.0f /* device_scale_factor*/, 1.0f,
                  Display::ROTATE_0, 60, gfx::Vector2dF(96.0, 96.0)));
  UpdateFromDisplayInfos(display_infos);
  previous_screen_ = Screen::GetScreen();
  Screen::SetScreenInstance(this);
}

ScopedScreenWin::~ScopedScreenWin() {
  Screen::SetScreenInstance(previous_screen_);
}

}  // namespace test
}  // namespace win
}  // namespace display
