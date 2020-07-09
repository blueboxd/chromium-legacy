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
#include "chrome/browser/ui/views/feature_promos/feature_promo_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/widget/widget.h"

TabGroupsIPHController::TabGroupsIPHController(Browser* browser,
                                               GetTabViewCallback get_tab_view)
    : tracker_(feature_engagement::TrackerFactory::GetForBrowserContext(
          browser->profile())),
      get_tab_view_(std::move(get_tab_view)) {
  DCHECK(tracker_);
  browser->tab_strip_model()->AddObserver(this);
}

TabGroupsIPHController::~TabGroupsIPHController() {
  if (!promo_widget_)
    return;

  // If we are destroyed before the promo, close it and stop observing it.
  promo_widget_->Close();
  HandlePromoClose();
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

  if (!tracker_->ShouldTriggerHelpUI(
          feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)) {
    return;
  }

  promo_widget_ = FeaturePromoBubbleView::CreateOwned(
                      get_tab_view_.Run(2), views::BubbleBorder::TOP_LEFT,
                      FeaturePromoBubbleView::ActivationAction::DO_NOT_ACTIVATE,
                      base::nullopt, IDS_TAB_GROUPS_NEW_GROUP_PROMO)
                      ->GetWidget();

  // We must notify the backend when the promo is dismissed. Observing
  // the promo's widget and notifying on close is the most
  // straightforward way to do this.
  widget_observer_.Add(promo_widget_);
}

void TabGroupsIPHController::OnTabGroupChanged(const TabGroupChange& change) {
  if (change.type != TabGroupChange::kCreated)
    return;

  tracker_->NotifyEvent(feature_engagement::events::kTabGroupCreated);
}

void TabGroupsIPHController::OnWidgetClosing(views::Widget* widget) {
  DCHECK_EQ(widget, promo_widget_);
  HandlePromoClose();
}

void TabGroupsIPHController::OnWidgetDestroying(views::Widget* widget) {
  DCHECK_EQ(widget, promo_widget_);
  HandlePromoClose();
}

void TabGroupsIPHController::HandlePromoClose() {
  widget_observer_.Remove(promo_widget_);
  promo_widget_ = nullptr;
  tracker_->Dismissed(feature_engagement::kIPHDesktopTabGroupsNewGroupFeature);
}
