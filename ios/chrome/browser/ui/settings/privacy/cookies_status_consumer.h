// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_STATUS_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_STATUS_CONSUMER_H_

#import <Foundation/Foundation.h>

@class CookiesStatusDescription;

// Consumer for Cookies status.
@protocol CookiesStatusConsumer

// Called when Cookies option has changed.
- (void)cookiesOptionChangedToDescription:
    (CookiesStatusDescription*)description;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_STATUS_CONSUMER_H_
