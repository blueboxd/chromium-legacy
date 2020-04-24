// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_coordinator.h"

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "components/google/core/common/google_util.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_user_settings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_observer_bridge.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_command_handler.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.h"
#import "ios/chrome/browser/ui/settings/google_services/manage_sync_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_passphrase_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/sync/sync_encryption_table_view_controller.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_browser_opener.h"
#include "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ManageSyncSettingsCoordinator () <
    ChromeIdentityBrowserOpener,
    ManageSyncSettingsCommandHandler,
    ManageSyncSettingsTableViewControllerPresentationDelegate,
    SyncObserverModelBridge> {
  // Sync observer.
  std::unique_ptr<SyncObserverBridge> _syncObserver;
}

// View controller.
@property(nonatomic, strong)
    ManageSyncSettingsTableViewController* viewController;
// Mediator.
@property(nonatomic, strong) ManageSyncSettingsMediator* mediator;
// Sync service.
@property(nonatomic, assign, readonly) syncer::SyncService* syncService;
// Dismiss callback for Web and app setting details view.
@property(nonatomic, copy) ios::DismissASMViewControllerBlock
    dismissWebAndAppSettingDetailsControllerBlock;

@end

@implementation ManageSyncSettingsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  if (self = [super initWithBaseViewController:navigationController
                                       browser:browser]) {
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  DCHECK(self.baseNavigationController);
  self.mediator = [[ManageSyncSettingsMediator alloc]
      initWithSyncService:self.syncService
          userPrefService:self.browser->GetBrowserState()->GetPrefs()];
  self.mediator.syncSetupService = SyncSetupServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
  self.mediator.commandHandler = self;
  self.viewController = [[ManageSyncSettingsTableViewController alloc]
      initWithStyle:UITableViewStyleGrouped];
  self.viewController.serviceDelegate = self.mediator;
  self.viewController.presentationDelegate = self;
  self.viewController.modelDelegate = self.mediator;
  self.mediator.consumer = self.viewController;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  _syncObserver.reset(new SyncObserverBridge(self, self.syncService));
}

#pragma mark - Properties

- (syncer::SyncService*)syncService {
  return ProfileSyncServiceFactory::GetForBrowserState(
      self.browser->GetBrowserState());
}

#pragma mark - Private

// Closes the Manage sync settings view controller.
- (void)closeManageSyncSettings {
  if (self.viewController.navigationController) {
    if (self.dismissWebAndAppSettingDetailsControllerBlock) {
      self.dismissWebAndAppSettingDetailsControllerBlock(NO);
      self.dismissWebAndAppSettingDetailsControllerBlock = nil;
    }
    [self.baseNavigationController popToViewController:self.viewController
                                              animated:NO];
    [self.baseNavigationController popViewControllerAnimated:YES];
  }
}

#pragma mark - ManageSyncSettingsTableViewControllerPresentationDelegate

- (void)manageSyncSettingsTableViewControllerWasRemoved:
    (ManageSyncSettingsTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate manageSyncSettingsCoordinatorWasRemoved:self];
}

#pragma mark - ChromeIdentityBrowserOpener

- (void)openURL:(NSURL*)url
              view:(UIView*)view
    viewController:(UIViewController*)viewController {
  OpenNewTabCommand* command =
      [OpenNewTabCommand commandWithURLFromChrome:net::GURLWithNSURL(url)];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler closeSettingsUIAndOpenURL:command];
}

#pragma mark - ManageSyncSettingsCommandHandler

- (void)openPassphraseDialog {
  DCHECK(self.mediator.shouldEncryptionItemBeEnabled);
  UIViewController<SettingsRootViewControlling>* controllerToPush;
  // If there was a sync error, prompt the user to enter the passphrase.
  // Otherwise, show the full encryption options.
  if (self.syncService->GetUserSettings()->IsPassphraseRequired()) {
    controllerToPush = [[SyncEncryptionPassphraseTableViewController alloc]
        initWithBrowserState:self.browser->GetBrowserState()];
  } else {
    controllerToPush = [[SyncEncryptionTableViewController alloc]
        initWithBrowserState:self.browser->GetBrowserState()];
  }
  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  controllerToPush.dispatcher = static_cast<
      id<ApplicationCommands, BrowserCommands, BrowsingDataCommands>>(
      self.browser->GetCommandDispatcher());
  [self.baseNavigationController pushViewController:controllerToPush
                                           animated:YES];
}

- (void)openTrustedVaultReauth {
  id<ApplicationCommands> applicationCommands =
      static_cast<id<ApplicationCommands>>(
          self.browser->GetCommandDispatcher());
  [applicationCommands
      showTrustedVaultReauthenticationFromViewController:self.viewController];
}

- (void)openWebAppActivityDialog {
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  base::RecordAction(base::UserMetricsAction(
      "Signin_AccountSettings_GoogleActivityControlsClicked"));
  self.dismissWebAndAppSettingDetailsControllerBlock =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->PresentWebAndAppSettingDetailsController(
              authService->GetAuthenticatedIdentity(), self.viewController,
              YES);
}

- (void)openDataFromChromeSyncWebPage {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(kSyncGoogleDashboardURL),
      GetApplicationContext()->GetApplicationLocale());
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:url];
  id<ApplicationCommands> handler = HandlerForProtocol(
      self.browser->GetCommandDispatcher(), ApplicationCommands);
  [handler closeSettingsUIAndOpenURL:command];
}

#pragma mark - SyncObserverModelBridge

- (void)onSyncStateChanged {
  if (!self.syncService->GetDisableReasons().Empty()) {
    [self closeManageSyncSettings];
  }
}

@end
