// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_H_

#include <vector>

#include "chrome/browser/ui/views/tabs/tab_animation_state.h"
#include "ui/gfx/geometry/size.h"

namespace gfx {
class Rect;
}

// Sizing info for individual tabs.
struct TabSizeInfo {
  // The width of pinned tabs.
  int pinned_tab_width;

  // The min width of active/inactive tabs.
  int min_active_width;
  int min_inactive_width;

  // The width of a standard tab, which is the largest size active or inactive
  // tabs ever have.
  int standard_width;
};

// Sizing info global to the tabstrip.
struct TabLayoutConstants {
  // The height of tabs.
  int tab_height;

  // The amount adjacent tabs overlap each other.
  int tab_overlap;
};

// Animation and sizing information for one tab.
struct TabLayoutInfo {
  TabAnimationState animation_state;
  TabSizeInfo size_info;
};

// Calculates and returns the bounds of the tabs. |width| is the available
// width to use for tab layout. This never sizes the tabs smaller then the
// minimum widths in TabSizeInfo, and as a result the calculated bounds may go
// beyond |width|.
std::vector<gfx::Rect> CalculateTabBounds(
    const TabLayoutConstants& layout_constants,
    const std::vector<TabLayoutInfo>& tabs,
    int width);

std::vector<gfx::Rect> CalculatePinnedTabBounds(
    const TabLayoutConstants& layout_constants,
    const std::vector<TabLayoutInfo>& pinned_tabs);

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_H_
