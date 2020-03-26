// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_COOKIES_VIEW_H_
#define IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_COOKIES_VIEW_H_

#import <UIKit/UIKit.h>

// View for displaying the controls for cookies.
@interface IncognitoCookiesView : UIView

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Switch to manage cookies blocking.
@property(nonatomic, strong, readonly) UISwitch* switchView;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_INCOGNITO_COOKIES_VIEW_H_
