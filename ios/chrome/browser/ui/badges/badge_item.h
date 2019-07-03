// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_ITEM_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_ITEM_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/badges/badge_type.h"

// Holds properties and values the UI needs to configure a badge button.
@protocol BadgeItem

// The type of the badge.
- (BadgeType)badgeType;
// Some badges may not be tappable if there is no action associated with it.
@property(nonatomic, assign, readonly, getter=isTappable) BOOL tappable;
// Whether this badge is in an accepted state.
@property(nonatomic, assign, readonly, getter=isAccepted) BOOL accepted;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_ITEM_H_
