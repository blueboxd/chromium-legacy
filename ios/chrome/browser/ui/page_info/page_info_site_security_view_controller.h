// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/page_info/page_info_site_security_description.h"

@protocol BrowserCommands;

// View Controller for displaying the site security.
@interface PageInfoSiteSecurityViewController : UIViewController

- (instancetype)initWitDescription:(PageInfoSiteSecurityDescription*)description
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// Handler used to navigate outside the page info.
@property(nonatomic, weak) id<BrowserCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_VIEW_CONTROLLER_H_
