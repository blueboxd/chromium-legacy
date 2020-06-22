// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/safety_check/safety_check_coordinator.h"

#include "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_navigation_commands.h"
#import "ios/chrome/browser/ui/settings/safety_check/safety_check_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/settings_navigation_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SafetyCheckCoordinator () <
    SafetyCheckNavigationCommands,
    SafetyCheckTableViewControllerPresentationDelegate>

// View controller for displaying the safety check screen.
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
  self.viewController = [[SafetyCheckTableViewController alloc]
      initWithStyle:UITableViewStylePlain];

  DCHECK(self.baseNavigationController);
  self.viewController.handler = self;
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  self.viewController.presentationDelegate = self;
}

#pragma mark - SafetyCheckTableViewControllerPresentationDelegate

- (void)safetyCheckTableViewControllerWasRemoved:
    (SafetyCheckTableViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate safetyCheckCoordinatorViewControllerWasRemoved:self];
}

@end
