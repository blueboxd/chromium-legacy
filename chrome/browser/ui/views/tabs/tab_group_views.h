// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_VIEWS_H_

#include <memory>

#include "components/tab_groups/tab_group_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"

class Tab;
class TabGroupHeader;
class TabGroupHighlight;
class TabGroupUnderline;
class TabStrip;

// The manager of all views associated with a tab group. This handles visual
// calculations and updates. Painting is done in TabStrip.
class TabGroupViews {
 public:
  TabGroupViews(TabStrip* tab_strip, const tab_groups::TabGroupId& group);
  ~TabGroupViews();

  tab_groups::TabGroupId group() const { return group_; }
  TabGroupHeader* header() const { return header_.get(); }
  TabGroupHighlight* highlight() const { return highlight_.get(); }
  TabGroupUnderline* underline() const { return underline_.get(); }

  // Updates the visuals of each view in preparation for repainting.
  void UpdateVisuals();

  // Returns the bounds of the entire group, including the header and all tabs.
  gfx::Rect GetBounds() const;

  // Returns the last tab in the group. Used for some visual calculations.
  Tab* GetLastTabInGroup() const;

  // Returns the group color.
  SkColor GetGroupColor() const;

  // Returns the tab highlight background color. Needed to layer painting for
  // the group background highlight.
  SkColor GetTabBackgroundColor() const;

  // Returns the group background color, which matches the non-active selected
  // tab color. Needed to layer painting for the group background highlight.
  SkColor GetGroupBackgroundColor() const;

 private:
  TabStrip* const tab_strip_;
  const tab_groups::TabGroupId group_;
  std::unique_ptr<TabGroupHeader> header_;
  std::unique_ptr<TabGroupHighlight> highlight_;
  std::unique_ptr<TabGroupUnderline> underline_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_VIEWS_H_
