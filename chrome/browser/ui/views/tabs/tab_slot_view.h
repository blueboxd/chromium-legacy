// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_VIEW_H_

#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "ui/views/view.h"

// View that can be laid out in the tabstrip.
class TabSlotView : public views::View {
 public:
  // Returns the TabSizeInfo for this View. Used by tab strip layout to size it
  // appropriately.
  virtual TabSizeInfo GetTabSizeInfo() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_SLOT_VIEW_H_
