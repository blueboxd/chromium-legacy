// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/credential_provider/sc_credential_list_coordinator.h"

#import <UIKit/UIKit.h>

#import "ios/chrome/credential_provider_extension/ui/credential_list_consumer.h"
#import "ios/chrome/credential_provider_extension/ui/credential_list_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SCCredentialListCoordinator () <CredentialListConsumerDelegate>
@property(nonatomic, strong) CredentialListViewController* viewController;
@end

@implementation SCCredentialListCoordinator
@synthesize baseViewController = _baseViewController;
@synthesize viewController = _viewController;

- (void)start {
  self.viewController = [[CredentialListViewController alloc] init];
  self.viewController.title = @"Autofill Chrome Password";
  self.viewController.delegate = self;
  [self.baseViewController setHidesBarsOnSwipe:NO];
  [self.baseViewController pushViewController:self.viewController animated:YES];
}

#pragma mark - CredentialListConsumerDelegate

- (void)navigationCancelButtonWasPressed:(UIButton*)button {
}

- (void)updateResultsWithFilter:(NSString*)filter {
  // TODO(crbug.com/1045454): Implement this method.
}

@end
