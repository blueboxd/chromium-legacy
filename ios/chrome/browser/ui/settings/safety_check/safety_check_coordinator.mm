// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_coordinator.h"

#include "base/mac/foundation_util.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_mediator.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SafetyCheckCoordinator () <
    SafetyCheckNavigationCommands,
    SafetyCheckTableViewControllerPresentationDelegate>

// Safety check mediator.
@property(nonatomic, strong) SafetyCheckMediator* mediator;

// The container view controller.
@property(nonatomic, strong) SafetyCheckTableViewController* viewController;

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
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  SafetyCheckTableViewController* viewController =
      [[SafetyCheckTableViewController alloc]
          initWithStyle:UITableViewStylePlain];
  self.viewController = viewController;

  self.mediator = [[SafetyCheckMediator alloc]
      initWithUserPrefService:self.browser->GetBrowserState()->GetPrefs()];
  self.mediator.consumer = self.viewController;
  [self.mediator updateConsumerCheckState];
  viewController.serviceDelegate = self.mediator.delegate;

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

@end
