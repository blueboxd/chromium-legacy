// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_GROUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_TAB_GROUP_CONTROLLER_H_

#include "base/optional.h"

class TabGroupId;

namespace content {
class WebContents;
}

class TabGroupController {
 public:
  virtual void CreateTabGroup(TabGroupId group) = 0;
  virtual void ChangeTabGroupContents(TabGroupId group) = 0;
  virtual void ChangeTabGroupVisuals(TabGroupId group) = 0;
  virtual void CloseTabGroup(TabGroupId group) = 0;

  // Methods from TabStipModel that are exposed to TabGroup.
  virtual base::Optional<TabGroupId> GetTabGroupForTab(int index) const = 0;
  virtual content::WebContents* GetWebContentsAt(int index) const = 0;
  virtual int GetTabCount() const = 0;

 protected:
  virtual ~TabGroupController() {}
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_GROUP_CONTROLLER_H_
