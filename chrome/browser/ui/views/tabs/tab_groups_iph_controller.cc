// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_groups_iph_controller.h"

#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ui/views/widget/widget.h"

TabGroupsIPHController::TabGroupsIPHController(Browser* browser)
    : tracker_(feature_engagement::TrackerFactory::GetForBrowserContext(
          browser->profile())) {
  DCHECK(tracker_);
  browser->tab_strip_model()->AddObserver(this);
}

TabGroupsIPHController::~TabGroupsIPHController() = default;

void TabGroupsIPHController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (change.type() == TabStripModelChange::kInserted &&
      tab_strip_model->count() >= 6) {
    tracker_->NotifyEvent(feature_engagement::events::kSixthTabOpened);

    if (tracker_->ShouldTriggerHelpUI(
            feature_engagement::kIPHDesktopTabGroupsNewGroupFeature)) {
      // Do nothing for now. TODO(crbug.com/1022943): show promo here.
      tracker_->Dismissed(
          feature_engagement::kIPHDesktopTabGroupsNewGroupFeature);
    }
  }
}

void TabGroupsIPHController::OnTabGroupChanged(const TabGroupChange& change) {
  if (change.type != TabGroupChange::kCreated)
    return;

  tracker_->NotifyEvent(feature_engagement::events::kTabGroupCreated);
}
