// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_DATA_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_DATA_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "third_party/skia/include/core/SkColor.h"

class TabGroupData {
 public:
  // Construct a TabGroupData with placeholder name and random color.
  TabGroupData();
  TabGroupData(const TabGroupData& other) = default;
  TabGroupData(TabGroupData&& other) = default;
  ~TabGroupData() = default;

  base::string16 title() const { return title_; }
  SkColor color() const { return color_; }
  bool empty() const { return tab_count_ == 0; }

  // Used to reference count the number of tabs in the group, so TabStripModel
  // knows when this group is safe to delete.
  void TabAdded();
  void TabRemoved();

 private:
  base::string16 title_;
  SkColor color_;
  int tab_count_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_DATA_H_
