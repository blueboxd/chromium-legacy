// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUPS_IPH_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUPS_IPH_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_controller.h"
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
class TabGroupsIPHController : public TabStripModelObserver {
 public:
  // Callback with an argument N that should return the Nth tab view in
  // the tab strip for bubble anchoring. If N is not valid, it should
  // return any tab view.
  using GetTabViewCallback = base::RepeatingCallback<views::View*(int)>;

  // |browser| is the browser window that this instance will track and
  // will show IPH in if needed. |promo_controller| is the window's
  // FeaturePromoControllerViews, used to start promos. |get_tab_view| should
  // get an appropriate tab to anchor the bubble in |browser|.
  TabGroupsIPHController(Browser* browser,
                         FeaturePromoController* promo_controller,
                         GetTabViewCallback get_tab_view);
  ~TabGroupsIPHController() override;

  // Whether the add-to-new-group item in the tab context menu should be
  // highlighted. Must be checked just before TabContextMenuOpened() is
  // called on the same task.
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

 private:
  FeaturePromoController* const promo_controller_;

  // The IPH backend for the profile.
  feature_engagement::Tracker* const tracker_;

  GetTabViewCallback get_tab_view_;

  // A handle given by |promo_controller_| if we show a context menu
  // promo. When destroyed this notifies |promo_controller_| we are
  // done.
  base::Optional<FeaturePromoController::PromoHandle> promo_handle_for_menu_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUPS_IPH_CONTROLLER_H_
