// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_

#include "components/exo/wm_helper.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/widget/widget.h"

namespace arc {
gfx::RectF ToChromeScale(const gfx::Rect& bounds) {
  DCHECK(exo::WMHelper::HasInstance());
  gfx::RectF bounds_f(bounds);
  bounds_f.Scale(1.0f /
                 exo::WMHelper::GetInstance()->GetDefaultDeviceScaleFactor());
  return bounds_f;
}

gfx::RectF ToChromeBounds(const gfx::Rect& bounds, views::Widget* widget) {
  DCHECK(widget);
  gfx::RectF chrome_bounds = ToChromeScale(bounds);

  // On Android side, content is rendered without considering height of
  // caption bar, e.g. Content is rendered at y:0 instead of y:32 where 32 is
  // height of caption bar. Add back height of caption bar here.
  if (widget->IsMaximized()) {
    chrome_bounds.Offset(
        0,
        widget->non_client_view()->frame_view()->GetBoundsForClientView().y());
  }

  return chrome_bounds;
}
}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ACCESSIBILITY_GEOMETRY_UTIL_H_
