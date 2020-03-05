// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/chrome_root_coordinator.h"

@protocol ApplicationCommands;
@protocol BrowsingDataCommands;
@protocol TabSwitcher;

class Browser;

@interface TabGridCoordinator : ChromeRootCoordinator

- (instancetype)initWithWindow:(UIWindow*)window
     applicationCommandEndpoint:
         (id<ApplicationCommands>)applicationCommandEndpoint
    browsingDataCommandEndpoint:
        (id<BrowsingDataCommands>)browsingDataCommandEndpoint
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithWindow:(UIWindow*)window NS_UNAVAILABLE;

@property(nonatomic, readonly) id<TabSwitcher> tabSwitcher;

@property(nonatomic, assign) Browser* regularBrowser;
@property(nonatomic, assign) Browser* incognitoBrowser;

// The view controller, if any, that is active.
@property(nonatomic, readonly, strong) UIViewController* activeViewController;

// The view controller that is doing the view controller swapping.
// This may or may not be the same as |activeViewController|.
@property(nonatomic, readonly, strong) UIViewController* viewController;

// If this property is YES, calls to |showTabSwitcher:completion:| and
// |showTabViewController:completion:| will present the given view controllers
// without animation.  This should only be used by unittests.
@property(nonatomic, readwrite, assign) BOOL animationsDisabledForTesting;

// Stops all child coordinators then calls |completion|. |completion| is called
// whether or not child coordinators exist.
- (void)stopChildCoordinatorsWithCompletion:(ProceduralBlock)completion;

// Displays the given TabSwitcher, replacing any TabSwitchers or view
// controllers that may currently be visible.
- (void)showTabSwitcher:(id<TabSwitcher>)tabSwitcher;

// Displays the given view controller, replacing any TabSwitchers or other view
// controllers that may currently be visible.  Runs the given |completion| block
// after the view controller is visible.
- (void)showTabViewController:(UIViewController*)viewController
                   completion:(ProceduralBlock)completion;

// Perform any initial setup required for the appearance of |tabSwitcher|.
- (void)prepareToShowTabSwitcher:(id<TabSwitcher>)tabSwitcher;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_COORDINATOR_H_
