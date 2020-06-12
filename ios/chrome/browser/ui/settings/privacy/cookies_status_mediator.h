// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_STATUS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_STATUS_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol CookiesStatusConsumer;
@class CookiesStatusDescription;
class HostContentSettingsMap;
class PrefService;

// The mediator is pushing the data for Cookies related views to the
// consumer.
@interface CookiesStatusMediator : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer.
- (instancetype)initWithPrefService:(PrefService*)prefService
                        settingsMap:(HostContentSettingsMap*)settingsMap
    NS_DESIGNATED_INITIALIZER;

// The consumer for this mediator.
@property(nonatomic, weak) id<CookiesStatusConsumer> consumer;

// Returns a configuration for Cookies related views to the coordinator.
- (CookiesStatusDescription*)cookiesDescription;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_STATUS_MEDIATOR_H_
