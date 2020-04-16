// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/privacy_coordinator.h"

#import "base/mac/foundation_util.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/clear_browsing_data/clear_browsing_data_ui_delegate.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_coordinator.h"
#import "ios/chrome/browser/ui/settings/privacy/handoff_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/privacy/privacy_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrivacyCoordinator () <
    ClearBrowsingDataUIDelegate,
    PrivacyCookiesCoordinatorDelegate,
    PrivacyNavigationCommands,
    PrivacyTableViewControllerPresentationDelegate>

@property(nonatomic, strong) id<ApplicationCommands> handler;
@property(nonatomic, strong) PrivacyTableViewController* viewController;
@property(nonatomic, strong) PrivacyCookiesCoordinator* cookiesCoordinator;

@end

@implementation PrivacyCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if ([super initWithBaseViewController:navigationController browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.handler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                    ApplicationCommands);
  self.viewController =
      [[PrivacyTableViewController alloc] initWithBrowser:self.browser];

  DCHECK(self.baseNavigationController);
  self.viewController.handler = self;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  self.viewController.presentationDelegate = self;
  self.viewController.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());
}

- (void)stop {
  self.viewController = nil;
  [self.cookiesCoordinator stop];
  self.cookiesCoordinator = nil;
}

#pragma mark - PrivacyTableViewControllerPresentationDelegate

- (void)privacyTableViewControllerWasRemoved:
    (PrivacyTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate privacyCoordinatorViewControllerWasRemoved:self];
}

#pragma mark - PrivacyCookiesCoordinatorDelegate

- (void)privacyCookiesCoordinatorViewControllerWasRemoved:
    (PrivacyCookiesCoordinator*)coordinator {
  DCHECK(self.cookiesCoordinator);
  [coordinator stop];
  coordinator = nil;
}

#pragma mark - PrivacyNavigationCommands

- (void)showHandoff {
  HandoffTableViewController* viewController =
      [[HandoffTableViewController alloc]
          initWithBrowserState:self.browser->GetBrowserState()];
  viewController.dispatcher = self.viewController.dispatcher;
  [self.baseNavigationController pushViewController:viewController
                                           animated:YES];
}

- (void)showClearBrowsingData {
  ClearBrowsingDataTableViewController* viewController =
      [[ClearBrowsingDataTableViewController alloc]
          initWithBrowser:self.browser];
  viewController.dispatcher = self.viewController.dispatcher;
  viewController.delegate = self;
  [self.baseNavigationController pushViewController:viewController
                                           animated:YES];
}

- (void)showCookies {
  self.cookiesCoordinator = [[PrivacyCookiesCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser];
  self.cookiesCoordinator.delegate = self;
  [self.cookiesCoordinator start];
}

#pragma mark - PrivacyTableViewControllerPresentationDelegate

- (void)privacyTableViewControllerViewControllerWasRemoved:
    (PrivacyTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate privacyCoordinatorViewControllerWasRemoved:self];
}

#pragma mark - ClearBrowsingDataUIDelegate

- (void)openURL:(const GURL&)URL {
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL];
  [self.handler closeSettingsUIAndOpenURL:command];
}

- (void)dismissClearBrowsingData {
  SettingsNavigationController* navigationController =
      base::mac::ObjCCastStrict<SettingsNavigationController>(
          self.viewController.navigationController);
  [navigationController closeSettings];
}
@end
