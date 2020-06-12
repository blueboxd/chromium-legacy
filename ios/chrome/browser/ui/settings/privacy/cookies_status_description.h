// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_STATUS_DESCRIPTION_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_STATUS_DESCRIPTION_H_

#import <Foundation/Foundation.h>

// Config to display Cookies status.
@interface CookiesStatusDescription : NSObject

@property(nonatomic, copy) NSString* headerDescription;
@property(nonatomic, copy) NSString* footerDescription;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_STATUS_DESCRIPTION_H_
