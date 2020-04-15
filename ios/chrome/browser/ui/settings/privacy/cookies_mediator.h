// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_MEDIATOR_H_

#import <Foundation/Foundation.h>

@protocol PrivacyCookiesConsumer;

// The mediator is pushing the data for the root of the Cookies screen to the
// consumer.
@interface PrivacyCookiesMediator : NSObject

// The consumer for this mediator.
@property(nonatomic, weak) id<PrivacyCookiesConsumer> consumer;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_MEDIATOR_H_
