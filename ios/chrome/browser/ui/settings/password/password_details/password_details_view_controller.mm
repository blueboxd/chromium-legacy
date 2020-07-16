// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/password/password_details/password_details_view_controller.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_consumer.h"
#import "ios/chrome/browser/ui/settings/password/password_details/password_details_table_view_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordDetailsViewController ()

// Password which is shown on the screen.
@property(nonatomic, strong) PasswordDetails* password;

@end

@implementation PasswordDetailsViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.tableView.accessibilityIdentifier = kPasswordDetailsViewControllerId;
  self.tableView.allowsSelectionDuringEditing = YES;

  [self loadModel];
}

#pragma mark - ChromeTableViewController

- (void)loadModel {
  [super loadModel];
}

#pragma mark - PasswordDetailsConsumer

- (void)setPassword:(PasswordDetails*)password {
  _password = password;
  [self reloadData];
}

@end
