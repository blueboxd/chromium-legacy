// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_APP_LAUNCHER_APP_LAUNCHER_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_APP_LAUNCHER_APP_LAUNCHER_COORDINATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/app_launcher/app_launcher_tab_helper_delegate.h"
#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// A coordinator that handles UI related to launching apps.
@interface AppLauncherCoordinator
    : ChromeCoordinator <AppLauncherTabHelperDelegate>

// Unavailable, use -initWithBaseViewController:browser:.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;
// Unavailable, use -initWithBaseViewController:browser:.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_APP_LAUNCHER_APP_LAUNCHER_COORDINATOR_H_
