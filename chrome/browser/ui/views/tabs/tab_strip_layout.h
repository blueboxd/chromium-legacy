// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_H_

#include <vector>

#include "chrome/browser/ui/views/tabs/tab_strip_layout_types.h"
#include "chrome/browser/ui/views/tabs/tab_width_constraints.h"

namespace gfx {
class Rect;
}

// Calculates and returns the bounds of the tabs. |width| is the available
// width to use for tab layout. This never sizes the tabs smaller then the
// minimum widths in TabSizeInfo, and as a result the calculated bounds may go
// beyond |width|.
std::vector<gfx::Rect> CalculateTabBounds(
    const TabLayoutConstants& layout_constants,
    const std::vector<TabWidthConstraints>& tabs,
    int width);

std::vector<gfx::Rect> CalculatePinnedTabBounds(
    const TabLayoutConstants& layout_constants,
    const std::vector<TabWidthConstraints>& pinned_tabs);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_H_
