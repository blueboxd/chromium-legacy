// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_MEDIATOR_H_

#import <Foundation/Foundation.h>

#include "ios/chrome/browser/main/browser.h"
#import "ios/web/public/web_state_observer_bridge.h"

@protocol PageInfoConsumer;

// The mediator is pushing the data for the root of the info page page to the
// consumer.
@interface PageInfoMediator : NSObject

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The consumer for this mediator.
@property(nonatomic, weak) id<PageInfoConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_MEDIATOR_H_
