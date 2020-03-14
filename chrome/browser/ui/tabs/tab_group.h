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
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"

class TabGroupController;

// The metadata and state of a tab group. This handles state changes that are
// specific to tab groups and not grouped tabs. The latter (i.e. the groupness
// state of a tab) is handled by TabStripModel, which also notifies TabStrip of
// any grouped tab state changes that need to be reflected in the view. TabGroup
// handles similar notifications for tab group state changes.
class TabGroup {
 public:
  TabGroup(TabGroupController* controller,
           const tab_groups::TabGroupId& id,
           const tab_groups::TabGroupVisualData& visual_data);
  ~TabGroup();

  const tab_groups::TabGroupId& id() const { return id_; }
  const tab_groups::TabGroupVisualData* visual_data() const {
    return visual_data_.get();
  }

  // Sets the visual data of the tab group. |is_customized| is true when this
  // method is called from the user explicitly setting the data and defaults to
  // false for callsites that may set the data such as tab restore.
  void SetVisualData(const tab_groups::TabGroupVisualData& visual_data,
                     bool is_customized = false);

  // Returns a user-visible string describing the contents of the group, such as
  // "Google Search and 3 other tabs". Used for accessibly describing the group,
  // as well as for displaying in context menu items and tooltips when the group
  // is unnamed.
  base::string16 GetContentString() const;

  // Updates internal bookkeeping for group contents, and notifies the
  // controller that contents changed when a tab is added.
  void AddTab();

  // Updates internal bookkeeping for group contents, and notifies the
  // controller that contents changed when a tab is removed.
  void RemoveTab();

  // Returns whether the group has no tabs.
  bool IsEmpty() const;

  // Returns whether the user has explicitly set the visual data themselves.
  bool IsCustomized() const;

  // Returns the model indices of all tabs in this group. Notably does not rely
  // on the TabGroup's internal metadata, but rather traverses directly through
  // the tabs in TabStripModel.
  std::vector<int> ListTabs() const;

 private:
  TabGroupController* controller_;

  tab_groups::TabGroupId id_;
  std::unique_ptr<tab_groups::TabGroupVisualData> visual_data_;

  int tab_count_ = 0;

  bool is_customized_ = false;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_H_
