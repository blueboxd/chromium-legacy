// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MAIN_CONTROLLER_PRIVATE_H_
#define IOS_CHROME_APP_MAIN_CONTROLLER_PRIVATE_H_

#import "base/ios/block_types.h"
#include "components/browsing_data/core/browsing_data_utils.h"
#import "ios/chrome/app/application_delegate/browser_launcher.h"
#import "ios/chrome/app/main_controller.h"
#include "ios/chrome/browser/browsing_data/browsing_data_remove_mask.h"

@class DeviceSharingManager;
class GURL;
@protocol TabSwitcher;

// Private methods and protocols that are made visible here for tests.
@interface MainController ()

// YES if the last time the app was launched was with a previous version.
@property(nonatomic, readonly) BOOL isFirstLaunchAfterUpgrade;

// Presents a promo's navigation controller.
- (void)showPromo:(UIViewController*)promo;

@end

// Methods that only exist for tests.
@interface MainController (TestingOnly)

@property(nonatomic, readonly) DeviceSharingManager* deviceSharingManager;
@property(nonatomic, retain) id<TabSwitcher> tabSwitcher;

// Tab switcher state.
@property(nonatomic, getter=isTabSwitcherActive) BOOL tabSwitcherActive;

// Sets the internal startup state to indicate that the launch was triggered
// by an external app opening the given URL.
- (void)setStartupParametersWithURL:(const GURL&)launchURL;

// Dismisses all modal dialogs, excluding the omnibox if |dismissOmnibox| is
// NO, then call |completion|.
- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

@end

#endif  // IOS_CHROME_APP_MAIN_CONTROLLER_PRIVATE_H_
