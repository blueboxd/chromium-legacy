
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation UserSigninCoordinator

- (instancetype)
    initWithBaseViewController:(UIViewController*)viewController
                       browser:(Browser*)browser
                      identity:(ChromeIdentity*)identity
                   accessPoint:(signin_metrics::AccessPoint)accessPoint
                   promoAction:(signin_metrics::PromoAction)promoAction {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    // TODO(crbug.com/971989): Needs implementation.
    NOTIMPLEMENTED();
  }
  return self;
}
@end
