// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/win/event_creation_utils.h"

#include <windows.h>
#include <winuser.h>

#include <algorithm>

#include "base/numerics/ranges.h"
#include "ui/gfx/geometry/point.h"

namespace ui {

bool SendMouseEvent(const gfx::Point& point, int flags) {
  INPUT input = {INPUT_MOUSE};
  // Get the max screen coordinate for use in computing the normalized absolute
  // coordinates required by SendInput.
  const int max_x = ::GetSystemMetrics(SM_CXSCREEN) - 1;
  const int max_y = ::GetSystemMetrics(SM_CYSCREEN) - 1;
  int screen_x = base::ClampToRange(point.x(), 0, max_x);
  int screen_y = base::ClampToRange(point.y(), 0, max_y);

  // Form the input data containing the normalized absolute coordinates. As of
  // Windows 10 Fall Creators Update, moving to an absolute position of zero
  // does not work. It seems that moving to 1,1 does, though.
  input.mi.dx =
      static_cast<LONG>(std::max(1.0, std::ceil(screen_x * (65535.0 / max_x))));
  input.mi.dy =
      static_cast<LONG>(std::max(1.0, std::ceil(screen_y * (65535.0 / max_y))));
  input.mi.dwFlags = flags;
  return ::SendInput(1, &input, sizeof(input)) == 1;
}

}  // namespace ui
