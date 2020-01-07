// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_CONFIG_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_CONFIG_H_

#import <UIKit/UIKit.h>

// Types of the different actions the page info button can have.
typedef NS_ENUM(NSUInteger, PageInfoButtonAction) {
  // No action.
  PageInfoButtonActionNone,
  // Show the help page.
  PageInfoButtonActionShowHelp,
  // Reload the page.
  PageInfoButtonActionReload,
};

// Config for the information displayed by the PageInfo.
@interface PageInfoConfig : NSObject

@property(nonatomic, copy) NSString* title;
@property(nonatomic, copy) NSString* message;
@property(nonatomic, strong) UIImage* image;
@property(nonatomic, assign) PageInfoButtonAction buttonAction;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_CONFIG_H_
