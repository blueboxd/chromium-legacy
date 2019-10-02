// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/passwords/password_breach_coordinator.h"

#import "ios/chrome/browser/ui/passwords/password_breach_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PasswordBreachCoordinator ()

// The main view controller for this coordinator.
@property(nonatomic, strong) PasswordBreachViewController* viewController;

@end

@implementation PasswordBreachCoordinator

- (void)start {
  [super start];
  self.viewController = [[PasswordBreachViewController alloc] init];
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.viewController.presentingViewController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.viewController = nil;
  [super stop];
}

@end
