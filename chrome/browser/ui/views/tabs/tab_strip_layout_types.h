// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_TYPES_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_TYPES_H_

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

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_STRIP_LAYOUT_TYPES_H_
