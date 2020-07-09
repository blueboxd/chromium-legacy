// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUPS_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUPS_IPH_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace feature_engagement {
class Tracker;
}

namespace views {
class View;
}

class Browser;

// Manages in-product help for tab groups. Watches for relevant events
// in a browser window, communicates them to the IPH backend, and
// displays IPH when appropriate.
//
// This doesn't actually show the IPH yet. TODO(crbug.com/1022943): show
// a promo.
class TabGroupsIPHController : public TabStripModelObserver,
                               public views::WidgetObserver {
 public:
  using GetTabViewCallback = base::RepeatingCallback<views::View*(int)>;

  // |browser| is the browser window that this instance will track and
  // will show IPH in if needed. |get_tab_view| is a callback with an
  // argument N that should return the Nth tab view in the tab strip for
  // bubble anchoring. If N is not valid, it should return any tab view.
  TabGroupsIPHController(Browser* browser, GetTabViewCallback get_tab_view);
  ~TabGroupsIPHController() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 private:
  void HandlePromoClose();

  // The IPH backend for the profile.
  feature_engagement::Tracker* const tracker_;

  GetTabViewCallback get_tab_view_;

  // The promo widget. Only non-null while it is showing.
  views::Widget* promo_widget_ = nullptr;

  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUPS_IPH_CONTROLLER_H_
