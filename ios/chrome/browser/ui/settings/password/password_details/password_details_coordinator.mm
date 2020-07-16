// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator.h"

#include "base/mac/foundation_util.h"
#include "components/autofill/core/common/password_form.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_coordinator_delegate.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_handler.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_mediator.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_view_controller.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordDetailsCoordinator () <PasswordDetailsHandler> {
  autofill::PasswordForm _password;
}

// Main view controller for this coordinator.
@property(nonatomic, strong) PasswordDetailsViewController* viewController;

// Main mediator for this coordinator.
@property(nonatomic, strong) PasswordDetailsMediator* mediator;

@end

@implementation PasswordDetailsCoordinator

@synthesize baseNavigationController = _baseNavigationController;

- (instancetype)initWithBaseNavigationController:
                    (UINavigationController*)navigationController
                                        password:(const autofill::PasswordForm&)
                                                     password {
  self = [super initWithBaseViewController:navigationController browser:nil];
  if (self) {
    _password = password;
    _baseNavigationController = navigationController;
  }
  return self;
}

- (void)start {
  UITableViewStyle style = base::FeatureList::IsEnabled(kSettingsRefresh)
                               ? UITableViewStylePlain
                               : UITableViewStyleGrouped;

  self.viewController =
      [[PasswordDetailsViewController alloc] initWithStyle:style];

  self.mediator = [[PasswordDetailsMediator alloc] initWithPassword:_password];
  self.mediator.consumer = self.viewController;
  self.viewController.handler = self;

  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
}

- (void)stop {
  self.mediator = nil;
  self.viewController = nil;
}

#pragma mark - PasswordDetailsHandler

- (void)passwordDetailsViewControllerDidDisappear {
  [self.delegate passwordDetailsCoordinatorDidRemove:self];
}

@end
