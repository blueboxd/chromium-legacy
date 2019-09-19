// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_

#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "ui/views/view.h"

// View for tab group underlines in the tab strip, which are markers of group
// members. There is one underline for each group, which is included in the tab
// strip flow and positioned across all tabs in the group.
class TabGroupUnderline : public views::View {
 public:
  TabGroupUnderline(TabStrip* tab_strip, TabGroupId group);

  TabGroupId group() const { return group_; }

  // Updates the bounds and color of the underline for painting.
  void UpdateVisuals();

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  TabStrip* const tab_strip_;
  const TabGroupId group_;

  // The underline color is the group color.
  SkColor GetColor();

  // The underline starts at the left edge of the header chip.
  int GetStart();

  // The underline ends at the right edge of the last grouped tab's close
  // button.
  int GetEnd();

  DISALLOW_COPY_AND_ASSIGN(TabGroupUnderline);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_UNDERLINE_H_
