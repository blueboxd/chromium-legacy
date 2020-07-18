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

  // Whether the add-to-new-group item in the tab context menu should be
  // highlighted. Must be checked before TabContextMenuOpened() is
  // called.
  bool ShouldHighlightContextMenuItem();

  // Should be called when a tab context menu is opened.
  void TabContextMenuOpened();

  // Likewise, should be called when a tab context menu is closed.
  void TabContextMenuClosed();

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;
  void OnTabGroupChanged(const TabGroupChange& change) override;

  // views::WidgetObserver:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  views::Widget* promo_widget_for_testing() { return promo_widget_; }

 private:
  void HandlePromoClose();

  // Notify the backend that the promo finished.
  void Dismissed();

  // The IPH backend for the profile.
  feature_engagement::Tracker* const tracker_;

  GetTabViewCallback get_tab_view_;

  // The promo bubble's widget. Only non-null while it is showing.
  views::Widget* promo_widget_ = nullptr;

  // True if the user opened a tab context menu while the bubble was
  // showing. A promo is now showing in the menu. When true, we wait
  // until the menu is closed to notify the backend of dismissal.
  bool showing_in_menu_ = false;

  ScopedObserver<views::Widget, views::WidgetObserver> widget_observer_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUPS_IPH_CONTROLLER_H_
