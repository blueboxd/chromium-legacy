// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/privacy/cookies_coordinator.h"

#include "base/logging.h"
#import "ios/chrome/browser/ui/settings/privacy/cookies_view_controller.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface PrivacyCookiesCoordinator () <
    PrivacyCookiesViewControllerPresentationDelegate>

@property(nonatomic, strong) PrivacyCookiesViewController* viewController;

@end

@implementation PrivacyCookiesCoordinator

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
  self.viewController = [[PrivacyCookiesViewController alloc]
      initWithStyle:UITableViewStylePlain];
  [self.baseNavigationController pushViewController:self.viewController
                                           animated:YES];
  self.viewController.presentationDelegate = self;
}

- (void)stop {
  self.viewController = nil;
}

#pragma mark - PrivacyCookiesViewControllerPresentationDelegate

- (void)privacyCookiesViewControllerWasRemoved:
    (PrivacyCookiesViewController*)controller {
  DCHECK_EQ(self.viewController, controller);
  [self.delegate privacyCookiesCoordinatorViewControllerWasRemoved:self];
}

@end
