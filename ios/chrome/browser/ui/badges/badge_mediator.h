// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BADGES_BADGE_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_BADGES_BADGE_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol BadgeConsumer;
class WebStateList;

// A mediator object that updates the consumer when the state of badges changes.
@interface BadgeMediator : NSObject

- (instancetype)initWithConsumer:(id<BadgeConsumer>)consumer
                    webStateList:(WebStateList*)webStateList
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_BADGES_BADGE_MEDIATOR_H_
