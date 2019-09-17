// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_ACTION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_ACTION_HANDLER_H_

#import <UIKit/UIKit.h>

@protocol InfobarCommands;
@protocol BadgeDelegate;

// Handler for the actions associated with the different badge buttons.
@interface BadgeButtonActionHandler : NSObject

// The dispatcher for badge button actions.
@property(nonatomic, weak) id<InfobarCommands> dispatcher;

// The command handler.
@property(nonatomic, weak) id<BadgeDelegate> buttonActionDelegate;

// Action when a Passwords badge is tapped.
- (void)passwordsBadgeButtonTapped:(id)sender;

// Action when the overflow badge is tapped.
- (void)overflowBadgeButtonTapped:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_BUTTON_ACTION_HANDLER_H_
