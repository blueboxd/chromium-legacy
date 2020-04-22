// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_COMMANDS_H_

#import "ios/chrome/browser/ui/settings/privacy/cookies_consumer.h"

@class TableViewItem;

// Commands related to the Cookies state.
@protocol PrivacyCookiesCommands

// Updates Cookies settings with the given item type.
- (void)selectedCookiesSettingType:(CookiesSettingType)settingType;

// Deletes the site exception corresponding to the given TableViewItem.
- (void)deleteSiteExceptionWithItem:(TableViewItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_PRIVACY_COOKIES_COMMANDS_H_
