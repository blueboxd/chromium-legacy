// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_

// TODO(hirokisato) support multiple display.

namespace aura {
class Window;
}

namespace exo {
class WMHelper;
}

namespace gfx {
class RectF;
}

namespace views {
class Widget;
}

namespace arc {
// Given ARC pixels, returns DIPs in Chrome OS main display.
// This function only scales the bounds.
gfx::RectF ToChromeScale(const gfx::Rect& rect, exo::WMHelper* wm_helper);

// Given ARC pixels in screen coordinate, returns DIPs in Chrome OS main
// display. This function adjusts differences between ARC and Chrome.
gfx::RectF ToChromeBounds(const gfx::Rect& rect,
                          exo::WMHelper* wm_helper,
                          views::Widget* widget);

// Given DIPs in Chrome OS main display, scales it into pixels.
void ScaleDeviceFactor(gfx::RectF& rect, aura::Window* toplevel_window);
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_
