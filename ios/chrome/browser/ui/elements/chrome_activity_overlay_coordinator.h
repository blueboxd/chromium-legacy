// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_ELEMENTS_CHROME_ACTIVITY_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_ELEMENTS_CHROME_ACTIVITY_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/ui/coordinators/chrome_coordinator.h"

// Coordinator for displaying a UIActivityIndicatorView overlay over the current
// context.
@interface ChromeActivityOverlayCoordinator : ChromeCoordinator

// Use -initWithBaseViewController:browser:
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
    NS_UNAVAILABLE;

// Use -initWithBaseViewController:browser:
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:(ChromeBrowserState*)browserState
    NS_UNAVAILABLE;

// Text that will be shown above the UIActivityIndicatorView.
@property(nonatomic, copy) NSString* messageText;

// YES if the Coordinator is started. Meaning that the UIActivityIndicatorView
// is currently being displayed.
@property(nonatomic, assign) BOOL started;

@end

#endif  // IOS_CHROME_BROWSER_UI_ELEMENTS_CHROME_ACTIVITY_OVERLAY_COORDINATOR_H_
