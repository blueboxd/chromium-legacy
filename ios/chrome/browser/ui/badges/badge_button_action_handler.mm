// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_button_action_handler.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "ios/chrome/browser/infobars/infobar_metrics_recorder.h"
#import "ios/chrome/browser/infobars/infobar_type.h"
#import "ios/chrome/browser/ui/badges/badge_button.h"
#import "ios/chrome/browser/ui/badges/badge_delegate.h"
#import "ios/chrome/browser/ui/commands/infobar_commands.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation BadgeButtonActionHandler

- (void)passwordsBadgeButtonTapped:(id)sender {
  BadgeButton* badgeButton = base::mac::ObjCCastStrict<BadgeButton>(sender);
  MobileMessagesBadgeState state;
  if (badgeButton.accepted) {
    state = MobileMessagesBadgeState::Active;
    base::RecordAction(
        base::UserMetricsAction("MobileMessagesBadgeAcceptedTapped"));
  } else {
    state = MobileMessagesBadgeState::Inactive;
    base::RecordAction(
        base::UserMetricsAction("MobileMessagesBadgeNonAcceptedTapped"));
  }
  InfobarMetricsRecorder* metricsRecorder;
  if (badgeButton.badgeType == BadgeType::kBadgeTypePasswordSave) {
    metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypePasswordSave];
    [self.dispatcher displayModalInfobar:InfobarType::kInfobarTypePasswordSave];
  } else if (badgeButton.badgeType == BadgeType::kBadgeTypePasswordUpdate) {
    metricsRecorder = [[InfobarMetricsRecorder alloc]
        initWithType:InfobarType::kInfobarTypePasswordUpdate];
    [self.dispatcher
        displayModalInfobar:InfobarType::kInfobarTypePasswordUpdate];
  }
  [metricsRecorder recordBadgeTappedInState:state];
}

- (void)overflowBadgeButtonTapped:(id)sender {
  [self.buttonActionDelegate showOverflowMenu];
  // TODO(crbug.com/976901): Add metric for this action.
}

@end
