// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_APP_NAVIGATION_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_APP_NAVIGATION_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"

class ChromeBrowserState;
@class SettingsNavigationController;

// Handles the navigation through the application.
@protocol AppNavigation<NSObject>

// Presents a SignedInAccountsViewController for |browserState| on the top view
// controller.
- (void)presentSignedInAccountsViewControllerForBrowserState:
    (ChromeBrowserState*)browserState;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_APP_NAVIGATION_H_
