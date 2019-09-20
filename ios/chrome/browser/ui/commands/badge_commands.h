// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_BADGE_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_BADGE_COMMANDS_H_

#import <Foundation/Foundation.h>

@protocol BadgeItem;

@protocol BadgeCommands <NSObject>
// Displays the Badge popup menu showing |badgeItems|.
- (void)displayPopupMenuWithBadgeItems:(NSArray<id<BadgeItem>>*)badgeItems;
@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_BADGE_COMMANDS_H_
