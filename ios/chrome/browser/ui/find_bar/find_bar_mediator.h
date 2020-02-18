// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol FindInPageCommands;
class WebStateList;

// Mediator for the Find Bar and the Find In page feature. As this feature is
// currently being split off from BVC, this mediator will have more features
// added and is not an ideal example of the mediator pattern.
@interface FindBarMediator : NSObject

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
                      commandHandler:(id<FindInPageCommands>)commandHandler;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIND_BAR_FIND_BAR_MEDIATOR_H_
