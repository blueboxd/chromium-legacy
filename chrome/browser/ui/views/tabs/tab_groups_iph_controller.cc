// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_groups_iph_controller.h"

#include <utility>

#include "base/logging.h"
#include "base/optional.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_params.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_bubble_view.h"
#include "chrome/browser/ui/views/in_product_help/feature_promo_controller.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"

namespace {

// The index of the tab we'd like to anchor our bubble to.
constexpr int kPreferredAnchorTab = 2;

}  // namespace

TabGroupsIPHController::TabGroupsIPHController(
    Browser* browser,
    FeaturePromoController* promo_controller,
    GetTabViewCallback get_tab_view)
    : promo_controller_(promo_controller),
      tracker_(feature_engagement::TrackerFactory::GetForBrowserContext(
          browser->profile())),
      get_tab_view_(std::move(get_tab_view)) {
  DCHECK(promo_controller_);
  DCHECK(tracker_);
  browser->tab_strip_model()->AddObserver(this);
}

TabGroupsIPHController::~TabGroupsIPHController() = default;

bool TabGroupsIPHController::ShouldHighlightContextMenuItem() {
  // If the bubble is currently showing, the promo hasn't timed out yet.
  // The promo should continue into the context menu as a highlighted
  // item.
  return promo_controller_->BubbleIsShowing(
      feature_engagement::kIPHDesktopTabGroupsNewGroupFeature);
}

void TabGroupsIPHController::TabContextMenuOpened() {
  if (!promo_controller_->BubbleIsShowing(
          feature_engagement::kIPHDesktopTabGroupsNewGroupFeature))
    return;

  // Assume that the context menu code checked
  // ShouldHighlightContextMenuItem() and is correctly showing the promo
  // there.
  promo_handle_for_menu_ = promo_controller_->CloseBubbleAndContinuePromo(
      feature_engagement::kIPHDesktopTabGroupsNewGroupFeature);
}

void TabGroupsIPHController::TabContextMenuClosed() {
  if (!promo_handle_for_menu_)
    return;
  promo_handle_for_menu_.reset();
}

void TabGroupsIPHController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!(change.type() == TabStripModelChange::kInserted &&
        tab_strip_model->count() >= 6)) {
    return;
  }

  tracker_->NotifyEvent(feature_engagement::events::kSixthTabOpened);

  FeaturePromoBubbleParams bubble_params;
  bubble_params.body_string_specifier = IDS_TAB_GROUPS_NEW_GROUP_PROMO;
  bubble_params.anchor_view = get_tab_view_.Run(kPreferredAnchorTab);
  bubble_params.arrow = views::BubbleBorder::TOP_LEFT;
  promo_controller_->MaybeShowPromo(
      feature_engagement::kIPHDesktopTabGroupsNewGroupFeature,
      std::move(bubble_params));
}

void TabGroupsIPHController::OnTabGroupChanged(const TabGroupChange& change) {
  if (change.type != TabGroupChange::kCreated)
    return;

  tracker_->NotifyEvent(feature_engagement::events::kTabGroupCreated);
}
