// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_GUTS_H_
#define IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_GUTS_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/app/application_delegate/startup_information.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_forward.h"
#import "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/chrome/browser/web_state_list/web_state_list_observer_bridge.h"

@class TabModel;

@protocol SceneControllerGuts <WebStateListObserving>

- (void)dismissModalDialogsWithCompletion:(ProceduralBlock)completion
                           dismissOmnibox:(BOOL)dismissOmnibox;

- (void)openSelectedTabInMode:(ApplicationModeForTabOpening)tabOpeningTargetMode
            withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                   completion:(ProceduralBlock)completion;

- (void)openTabFromLaunchOptions:(NSDictionary*)launchOptions
              startupInformation:(id<StartupInformation>)startupInformation
                        appState:(AppState*)appState;

- (void)dismissModalsAndOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                            withUrlLoadParams:
                                (const UrlLoadParams&)urlLoadParams
                               dismissOmnibox:(BOOL)dismissOmnibox
                                   completion:(ProceduralBlock)completion;

- (BOOL)shouldOpenNTPTabOnActivationOfTabModel:(TabModel*)tabModel;

// TabSwitcherDelegate helpers

// Begins the process of dismissing the tab switcher with the given current
// model, switching which BVC is suspended if necessary, but not updating the
// UI.  The omnibox will be focused after the tab switcher dismissal is
// completed if |focusOmnibox| is YES.
- (void)beginDismissingTabSwitcherWithCurrentModel:(TabModel*)tabModel
                                      focusOmnibox:(BOOL)focusOmnibox;
// Completes the process of dismissing the tab switcher, removing it from the
// screen and showing the appropriate BVC.
- (void)finishDismissingTabSwitcher;

#pragma mark - AppNavigation helpers

// Presents a SignedInAccountsViewController for |browserState| on the top view
// controller.
- (void)presentSignedInAccountsViewControllerForBrowserState:
    (ios::ChromeBrowserState*)browserState;

// Clears incognito data that is specific to iOS and won't be cleared by
// deleting the browser state.
- (void)clearIOSSpecificIncognitoData;

- (void)activateBVCAndMakeCurrentBVCPrimary;

#pragma mark - iOS 12 compat

// Method called on SceneController when the scene disconnects. Exposed here for
// iOS 12 compatibility.
- (void)teardownUI;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_SCENE_CONTROLLER_GUTS_H_
