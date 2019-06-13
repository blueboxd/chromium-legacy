// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TABLE_VIEW_CONTROLLER_H_

#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// SendTabToSelfTableViewController represents the content for the
// Send Tab To Self Modal dialog.
@interface SendTabToSelfTableViewController : ChromeTableViewController

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTableViewStyle:(UITableViewStyle)style
                           appBarStyle:
                               (ChromeTableViewControllerStyle)appBarStyle
    NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_TABLE_VIEW_CONTROLLER_H_
