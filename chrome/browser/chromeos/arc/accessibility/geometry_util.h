// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_

// TODO(hirokisato) support multiple display.

namespace gfx {
class RectF;
}

namespace views {
class Widget;
}

namespace arc {
// Given ARC pixels, returns DIPs in Chrome OS main display.
// This function only scales the bounds.
gfx::RectF ToChromeScale(const gfx::Rect& rect);

// Given ARC pixels in screen coordinate, returns DIPs in Chrome OS main
// display. This function adjusts differences between ARC and Chrome.
gfx::RectF ToChromeBounds(const gfx::Rect& rect, views::Widget* widget);
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_
