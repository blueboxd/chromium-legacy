// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_LEGACY_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_LEGACY_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/infobars/infobar_type.h"

// States for the InfobarBadge.
typedef NS_OPTIONS(NSUInteger, InfobarBadgeState) {
  // Default state. e.g. the Banner is being displayed or there's nothing
  // presenting.
  InfobarBadgeStateNone = 0,
  // The InfobarBadge is selected. e.g. The InfobarBadge was tapped so the
  // InfobarModal has been presented.
  InfobarBadgeStateSelected = 1 << 0,
  // The InfobarBadge is accepted. e.g. The Infobar was accepted/confirmed, and
  // the Infobar action has taken place.
  InfobarBadgeStateAccepted = 1 << 1,
};

@protocol LegacyInfobarBadgeTabHelperDelegate

// Asks the delegate to display or stop displaying a badge.
- (void)displayBadge:(BOOL)display type:(InfobarType)infobarType;

// Current state for the displayed InfobarBadge.
@property(nonatomic, assign) InfobarBadgeState badgeState;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_LEGACY_INFOBAR_BADGE_TAB_HELPER_DELEGATE_H_
