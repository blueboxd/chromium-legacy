// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/badges/badge_coordinator.h"

#import "ios/chrome/browser/ui/badges/badge_mediator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface BadgeCoordinator ()

// The coordinator's mediator.
@property(nonatomic, strong) BadgeMediator* mediator;

@end

@implementation BadgeCoordinator

- (void)start {
  self.mediator = [[BadgeMediator alloc] initWithConsumer:self.viewController
                                             webStateList:self.webStateList];
}

- (void)stop {
  [self.mediator disconnect];
  self.mediator = nil;
}

@end
