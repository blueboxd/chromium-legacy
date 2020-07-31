// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_

#import <UIKit/UIKit.h>

class PrefService;
@protocol SafetyCheckConsumer;
@protocol SafetyCheckServiceDelegate;
@class SafetyCheckTableViewController;

// The mediator is pushing the data for the safety check to the consumer.
@interface SafetyCheckMediator : NSObject

// The consumer for the Safety Check mediator.
@property(nonatomic, weak) id<SafetyCheckConsumer> consumer;

// The delegate for the Safety Check mediator, handles row taps.
@property(nonatomic, weak) id<SafetyCheckServiceDelegate> delegate;

// Designated initializer. All the parameters should not be null.
// |userPrefService|: preference service from the browser state.
- (instancetype)initWithUserPrefService:(PrefService*)userPrefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Updates the consumer with the current check state.
- (void)updateConsumerCheckState;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SAFETY_CHECK_SAFETY_CHECK_MEDIATOR_H_
