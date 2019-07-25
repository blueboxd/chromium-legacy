// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_H_

#import "ios/chrome/browser/ui/elements/extended_touch_target_button.h"

#import "ios/chrome/browser/ui/badges/badge_type.h"

// A button that contains a badge icon image.
@interface BadgeButton : ExtendedTouchTargetButton

// Returns a BadgeButton with it's badgeType set to |badgeType|.
+ (instancetype)badgeButtonWithType:(BadgeType)badgeType;

// The badge type of the button.
@property(nonatomic, assign, readonly) BadgeType badgeType;

// Sets the badge color to the accepted color if |accepted| is YES or the
// default color if |accepted| is NO. Will animate change if |animated| is YES.
- (void)setAccepted:(BOOL)accepted animated:(BOOL)animated;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_H_
