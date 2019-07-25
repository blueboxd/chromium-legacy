// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_FACTORY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/badges/badge_type.h"

@class BadgeButtonActionHandler;
@class BadgeButton;

// BadgeButtonFactory Factory creates BadgButton objects with certain
// styles and configurations, depending on its type.
@interface BadgeButtonFactory : NSObject

- (instancetype)initWithActionHandler:(BadgeButtonActionHandler*)actionHandler
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Returns a properly configured BadgButton associated with |badgeType|.
- (BadgeButton*)getBadgeButtonForBadgeType:(BadgeType)badgeType;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_FACTORY_H_
