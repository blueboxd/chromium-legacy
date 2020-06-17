// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUPS_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUPS_IPH_CONTROLLER_H_

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"

namespace feature_engagement {
class Tracker;
}

class Browser;

// Manages in-product help for tab groups. Watches for relevant events
// in a browser window, communicates them to the IPH backend, and
// displays IPH when appropriate.
//
// This doesn't actually show the IPH yet. TODO(crbug.com/1022943): show
// a promo.
class TabGroupsIPHController : public TabStripModelObserver {
 public:
  explicit TabGroupsIPHController(Browser* browser);
  ~TabGroupsIPHController() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;

 private:
  // The IPH backend for the profile.
  feature_engagement::Tracker* const tracker_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUPS_IPH_CONTROLLER_H_
