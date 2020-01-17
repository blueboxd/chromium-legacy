// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_
#define IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_

#import <UIKit/UIKit.h>

#include "ios/chrome/app/startup/chrome_app_startup_parameters.h"
#import "ios/chrome/browser/crash_report/crash_restore_helper.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/public/provider/chrome/browser/user_feedback/user_feedback_provider.h"

@class BrowserViewController;
@class HistoryCoordinator;
@class SigninInteractionCoordinator;
@class TabGridCoordinator;
@protocol BrowserInterfaceProvider;
@protocol TabSwitcher;
class AppUrlLoadingService;

namespace ios {
class ChromeBrowserState;
}  // namespace ios

// Used to update the current BVC mode if a new tab is added while the tab
// switcher view is being dismissed.  This is different than ApplicationMode in
// that it can be set to |NONE| when not in use.
enum class TabSwitcherDismissalMode { NONE, NORMAL, INCOGNITO };

// TODO(crbug.com/1012697): Remove this protocol when SceneController is
// operational. Move the private internals back into MainController, and pass
// ownership of Scene-related objects to SceneController.
@protocol MainControllerGuts

// Coordinator for displaying history.
@property(nonatomic, strong) HistoryCoordinator* historyCoordinator;
@property(nonatomic, strong)
    SettingsNavigationController* settingsNavigationController;

// The application level component for url loading. Is passed down to
// browser state level UrlLoadingService instances.
@property(nonatomic, assign) AppUrlLoadingService* appURLLoadingService;

// The tab switcher command and the voice search commands can be sent by views
// that reside in a different UIWindow leading to the fact that the exclusive
// touch property will be ineffective and a command for processing both
// commands may be sent in the same run of the runloop leading to
// inconsistencies. Those two boolean indicate if one of those commands have
// been processed in the last 200ms in order to only allow processing one at
// a time.
// TODO(crbug.com/560296):  Provide a general solution for handling mutually
// exclusive chrome commands sent at nearly the same time.
@property(nonatomic, assign) BOOL isProcessingTabSwitcherCommand;
@property(nonatomic, assign) BOOL isProcessingVoiceSearchCommand;
// The SigninInteractionCoordinator to present Sign In UI. It is created the
// first time Sign In UI is needed to be presented and should not be destroyed
// while the UI is presented.
@property(nonatomic, strong)
    SigninInteractionCoordinator* signinInteractionCoordinator;

// If YES, the tab switcher is currently active.
@property(nonatomic, assign, getter=isTabSwitcherActive)
    BOOL tabSwitcherIsActive;

// YES while animating the dismissal of tab switcher.
@property(nonatomic, assign) BOOL dismissingTabSwitcher;

// Returns YES if the settings are presented, either from
// self.settingsNavigationController or from SigninInteractionCoordinator.
@property(nonatomic, assign, readonly, getter=isSettingsViewPresented)
    BOOL settingsViewPresented;

// If not NONE, the current BVC should be switched to this BVC on completion
// of tab switcher dismissal.
@property(nonatomic, assign)
    TabSwitcherDismissalMode modeToDisplayOnTabSwitcherDismissal;

// A property to track whether the QR Scanner should be started upon tab
// switcher dismissal. It can only be YES if the QR Scanner experiment is
// enabled.
@property(nonatomic, readwrite)
    NTPTabOpeningPostOpeningAction NTPActionAfterTabSwitcherDismissal;

// Parameters received at startup time when the app is launched from another
// app.
@property(nonatomic, strong) AppStartupParameters* startupParameters;

- (ProceduralBlock)completionBlockForTriggeringAction:
    (NTPTabOpeningPostOpeningAction)action;

// Keeps track of the restore state during startup.
@property(nonatomic, strong) CrashRestoreHelper* restoreHelper;

- (id<TabSwitcher>)tabSwitcher;
- (TabModel*)currentTabModel;
- (id<TabSwitcher>)tabSwitcher;
- (ios::ChromeBrowserState*)mainBrowserState;
- (ios::ChromeBrowserState*)currentBrowserState;
- (BrowserViewController*)currentBVC;
- (BrowserViewController*)mainBVC;
- (BrowserViewController*)otrBVC;
- (TabGridCoordinator*)mainCoordinator;
- (id<BrowserInterfaceProvider>)interfaceProvider;
- (void)startVoiceSearchInCurrentBVC;
- (void)showTabSwitcher;

// Sets |currentBVC| as the root view controller for the window.
- (void)displayCurrentBVCAndFocusOmnibox:(BOOL)focusOmnibox;

// Activates |mainBVC| and |otrBVC| and sets |currentBVC| as primary iff
// |currentBVC| can be made active.
- (void)activateBVCAndMakeCurrentBVCPrimary;

@end

#endif  // IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_
