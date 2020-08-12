// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_coordinator.h"

#include "base/mac/foundation_util.h"
#include "base/memory/scoped_refptr.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_check_manager_factory.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/command_dispatcher.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_coordinator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"
#import "ios/chrome/common/ui/elements/popover_label_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SafetyCheckCoordinator () <
    PasswordIssuesCoordinatorDelegate,
    SafetyCheckNavigationCommands,
    SafetyCheckTableViewControllerPresentationDelegate>

// Safety check mediator.
@property(nonatomic, strong) SafetyCheckMediator* mediator;

// The container view controller.
@property(nonatomic, strong) SafetyCheckTableViewController* viewController;

// Coordinator for passwords issues screen.
@property(nonatomic, strong)
    PasswordIssuesCoordinator* passwordIssuesCoordinator;

// Dispatcher which can handle changing passwords on sites.
@property(nonatomic, strong) id<ApplicationCommands> handler;

@end

@implementation SafetyCheckCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                         browser:(Browser*)browser {
  self = [super initWithBaseViewController:navigationController
                                   browser:browser];
  if (self) {
    _baseNavigationController = navigationController;
    _handler = HandlerForProtocol(self.browser->GetCommandDispatcher(),
                                  ApplicationCommands);
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  SafetyCheckTableViewController* viewController =
      [[SafetyCheckTableViewController alloc]
          initWithStyle:UITableViewStylePlain];
  self.viewController = viewController;

  scoped_refptr<IOSChromePasswordCheckManager> passwordCheckManager =
      IOSChromePasswordCheckManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.mediator = [[SafetyCheckMediator alloc]
      initWithUserPrefService:self.browser->GetBrowserState()->GetPrefs()
         passwordCheckManager:passwordCheckManager
                  authService:AuthenticationServiceFactory::GetForBrowserState(
                                  self.browser->GetBrowserState())
                  syncService:SyncSetupServiceFactory::GetForBrowserState(
                                  self.browser->GetBrowserState())];
  self.mediator.consumer = self.viewController;
  self.mediator.handler = self;
  self.viewController.serviceDelegate = self.mediator;
  self.viewController.presentationDelegate = self;

  DCHECK(self.baseNavigationController);
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

#pragma mark - SafetyCheckTableViewControllerPresentationDelegate

- (void)safetyCheckTableViewControllerDidRemove:
    (SafetyCheckTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate safetyCheckCoordinatorDidRemove:self];
}

#pragma mark - SafetyCheckNavigationCommands

- (void)showPasswordIssuesPage {
  IOSChromePasswordCheckManager* passwordCheckManager =
      IOSChromePasswordCheckManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState())
          .get();
  self.passwordIssuesCoordinator = [[PasswordIssuesCoordinator alloc]
      initWithBaseNavigationController:self.baseNavigationController
                               browser:self.browser
                  passwordCheckManager:passwordCheckManager];
  self.passwordIssuesCoordinator.delegate = self;
  self.passwordIssuesCoordinator.reauthModule = nil;
  [self.passwordIssuesCoordinator start];
}

- (void)showErrorInfoFrom:(UIButton*)buttonView
                 withText:(NSAttributedString*)text {
  PopoverLabelViewController* errorInfoPopover =
      [[PopoverLabelViewController alloc] initWithPrimaryAttributedString:text
                                                secondaryAttributedString:nil];

  errorInfoPopover.popoverPresentationController.sourceView = buttonView;
  errorInfoPopover.popoverPresentationController.sourceRect = buttonView.bounds;
  errorInfoPopover.popoverPresentationController.permittedArrowDirections =
      UIPopoverArrowDirectionAny;
  [self.viewController presentViewController:errorInfoPopover
                                    animated:YES
                                  completion:nil];
}

- (void)showUpdateOnAppStorePage {
  // TODO(crbug.com/1078782): Add navigation to App Store Chrome page.
}

- (void)showSafeBrowsingPreferencePage {
  // TODO(crbug.com/1078782): Add navigation to Safe Browsing preference page.
}

#pragma mark - PasswordIssuesCoordinatorDelegate

- (void)passwordIssuesCoordinatorDidRemove:
    (PasswordIssuesCoordinator*)coordinator {
  DCHECK_EQ(self.passwordIssuesCoordinator, coordinator);
  [self.passwordIssuesCoordinator stop];
  self.passwordIssuesCoordinator.delegate = nil;
  self.passwordIssuesCoordinator = nil;
}

- (BOOL)willHandlePasswordDeletion:(const autofill::PasswordForm&)password {
  return NO;
}

@end
