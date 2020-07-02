// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_issues_coordinator.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_presenter.h"
#import "ios/chrome/browser/ui/settings/password/password_issues_table_view_controller.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordIssuesCoordinator () <PasswordIssuesPresenter> {
  // Password check manager to power mediator.
  IOSChromePasswordCheckManager* _manager;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordIssuesTableViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordIssuesMediator* mediator;

@end

@implementation PasswordIssuesCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                            passwordCheckManager:
                                (IOSChromePasswordCheckManager*)manager {
  self = [super initWithBaseViewController:navigationController browser:nil];
  if (self) {
    _baseNavigationController = navigationController;
    _manager = manager;
  }
  return self;
}

- (void)start {
  [super start];
  // To start, a password check manager should be ready.
  DCHECK(_manager);

  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;

  self.viewController =
      [[PasswordIssuesTableViewController alloc] initWithStyle:style];

  self.mediator =
      [[PasswordIssuesMediator alloc] initWithPasswordCheckManager:_manager];
  self.mediator.consumer = self.viewController;
  self.viewController.presenter = self;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - PasswordIssuesPresenter

- (void)dismissPasswordIssuesTableViewController {
  [self.delegate passwordIssuesCoordinatorDidRemove:self];
}

- (void)presentPasswordIssueDetails:(id<PasswordIssue>)password {
  // TODO(crbug.com/1075494) - Show Password details page
}

@end
