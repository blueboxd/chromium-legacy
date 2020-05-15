// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_DESCRIPTION_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_DESCRIPTION_H_

#import <UIKit/UIKit.h>

// Types of the different actions the page info site security button can have.
typedef NS_ENUM(NSUInteger, PageInfoSiteSecurityButtonAction) {
  // No action.
  PageInfoSiteSecurityButtonActionNone,
  // Show the help page.
  PageInfoSiteSecurityButtonActionShowHelp,
  // Reload the page.
  PageInfoSiteSecurityButtonActionReload,
};

// Config for the information displayed by the page info site security.
@interface PageInfoSiteSecurityDescription : NSObject

@property(nonatomic, copy) NSString* siteURL;
@property(nonatomic, copy) NSString* status;
@property(nonatomic, copy) NSString* message;
// TODO(crbug.com/1038923): Remove this.
@property(nonatomic, strong) UIImage* legacyImage;
@property(nonatomic, copy) NSString* iconImageName;
@property(nonatomic, assign) PageInfoSiteSecurityButtonAction buttonAction;
@property(nonatomic, assign) BOOL isEmpty;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_DESCRIPTION_H_
