// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_

#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "ui/views/view.h"

class TabController;
class TabGroupVisualData;

namespace views {
class Label;
}

// View for tab group headers in the tab strip, which are tab-shaped markers of
// group boundaries. There is one header for each group, which is included in
// the tab strip flow and positioned left of the leftmost tab in the group.
class TabGroupHeader : public views::View {
 public:
  TabGroupHeader(TabController* controller, TabGroupId group);

  // Updates our visual state according to the TabGroupVisualData for our group.
  void VisualsChanged();

 private:
  TabController* const controller_;
  const TabGroupId group_;

  views::View* title_chip_;
  views::Label* title_;

  DISALLOW_COPY_AND_ASSIGN(TabGroupHeader);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_
