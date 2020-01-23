// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_CREDIT_CARD_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_CREDIT_CARD_TABLE_VIEW_CONTROLLER_H_

#include "ios/chrome/browser/browser_state/chrome_browser_state_forward.h"
#import "ios/chrome/browser/ui/settings/settings_root_table_view_controller.h"

class Browser;

// The table view for the Autofill settings.
@interface AutofillCreditCardTableViewController
    : SettingsRootTableViewController

// The designated initializer. |browser| must not be nil.
- (instancetype)initWithBrowser:(Browser*)browser NS_DESIGNATED_INITIALIZER;

// Use -initWithBrowser:.
- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

- (instancetype)initWithStyle:(UITableViewStyle)style NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_CREDIT_CARD_TABLE_VIEW_CONTROLLER_H_
