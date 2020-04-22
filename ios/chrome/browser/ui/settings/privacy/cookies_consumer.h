// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_CONSUMER_H_

#import "ios/chrome/browser/ui/list_model/list_model.h"

typedef NS_ENUM(NSInteger, CookiesSettingType) {
  SettingTypeAllowCookies,
  SettingTypeBlockThirdPartyCookiesIncognito,
  SettingTypeBlockThirdPartyCookies,
  SettingTypeBlockAllCookies,
};

@class TableViewItem;

// A consumer for Cookies settings changes.
@protocol PrivacyCookiesConsumer

// Called when a cookie setting option was selected.
- (void)cookiesSettingsOptionSelected:(CookiesSettingType)settingType;

// Inserts site exceptions item into the model, previous items aren't deleted.
- (void)insertSiteExceptionsItems:(NSArray<TableViewItem*>*)items;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_CONSUMER_H_
