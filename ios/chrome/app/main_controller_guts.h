// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_
#define IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_forward.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"
#import "ios/chrome/browser/crash_report/crash_restore_helper.h"

@class BrowserViewController;
@class BrowserViewWrangler;
@class TabGridCoordinator;
@protocol BrowserInterfaceProvider;
@protocol TabSwitcher;
class AppUrlLoadingService;

// TODO(crbug.com/1012697): Remove this protocol when SceneController is
// operational. Move the private internals back into MainController, and pass
// ownership of Scene-related objects to SceneController.
@protocol MainControllerGuts

// The application level component for url loading. Is passed down to
// browser state level UrlLoadingService instances.
@property(nonatomic, assign) AppUrlLoadingService* appURLLoadingService;

// If YES, the tab switcher is currently active.
@property(nonatomic, assign, getter=isTabSwitcherActive)
    BOOL tabSwitcherIsActive;

// YES while animating the dismissal of tab switcher.
@property(nonatomic, assign) BOOL dismissingTabSwitcher;

// Parameters received at startup time when the app is launched from another
// app.
@property(nonatomic, strong) AppStartupParameters* startupParameters;

// Keeps track of the restore state during startup.
@property(nonatomic, strong) CrashRestoreHelper* restoreHelper;

- (BrowserViewWrangler*)browserViewWrangler;
- (id<TabSwitcher>)tabSwitcher;
- (TabModel*)currentTabModel;
- (ios::ChromeBrowserState*)mainBrowserState;
- (ios::ChromeBrowserState*)currentBrowserState;
- (BrowserViewController*)currentBVC;
- (BrowserViewController*)mainBVC;
- (BrowserViewController*)otrBVC;
- (TabGridCoordinator*)mainCoordinator;
- (id<BrowserInterfaceProvider>)interfaceProvider;

- (void)removeBrowsingDataForBrowserState:(ios::ChromeBrowserState*)browserState
                               timePeriod:(browsing_data::TimePeriod)timePeriod
                               removeMask:(BrowsingDataRemoveMask)removeMask
                          completionBlock:(ProceduralBlock)completionBlock;

@end

#endif  // IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_
