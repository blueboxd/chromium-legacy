// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <vector>

#include "base/strings/string16.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_group_visual_data.h"

class TabGroupController;

// The metadata and state of a tab group. This handles state changes that are
// specific to tab groups and not grouped tabs. The latter (i.e. the groupness
// state of a tab) is handled by TabStripModel, which also notifies TabStrip of
// any grouped tab state changes that need to be reflected in the view. TabGroup
// handles similar notifications for tab group state changes.
class TabGroup {
 public:
  TabGroup(TabGroupController* controller,
           TabGroupId id,
           TabGroupVisualData visual_data);
  ~TabGroup();

  TabGroupId id() const { return id_; }
  TabGroupVisualData* visual_data() const { return visual_data_.get(); }
  void SetVisualData(TabGroupVisualData visual_data);

  // Returns the user-visible group title that will be displayed in context
  // menus and tooltips. Generates a descriptive placeholder if the user has
  // not yet named the group, otherwise uses the group's name.
  base::string16 GetDisplayedTitle() const;

  // Updates internal bookkeeping for group contents, and notifies the
  // controller that contents changed when a tab is added.
  void AddTab();

  // Updates internal bookkeeping for group contents, and notifies the
  // controller that contents changed when a tab is removed.
  void RemoveTab();

  // Returns whether the group has no tabs.
  bool IsEmpty() const;

  // Returns the model indices of all tabs in this group. Notably does not rely
  // on the TabGroup's internal metadata, but rather traverses directly through
  // the tabs in TabStripModel.
  std::vector<int> ListTabs() const;

 private:
  TabGroupController* controller_;

  TabGroupId id_;
  std::unique_ptr<TabGroupVisualData> visual_data_;

  int tab_count_ = 0;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_H_
