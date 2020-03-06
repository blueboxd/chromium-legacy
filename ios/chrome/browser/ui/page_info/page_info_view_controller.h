// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/page_info/page_info_consumer.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

@protocol PageInfoNavigationCommands;

// View Controller for displaying the page info.
@interface PageInfoViewController : ChromeTableViewController <PageInfoConsumer>

// Handler used to navigate inside the page info.
@property(nonatomic, weak) id<PageInfoNavigationCommands> handler;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_VIEW_CONTROLLER_H_
