// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@implementation SigninCoordinator

+ (instancetype)
    userSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                        browser:(Browser*)browser
                                       identity:(ChromeIdentity*)identity
                                    accessPoint:(AccessPoint)accessPoint
                                    promoAction:(PromoAction)promoAction {
  return
      [[UserSigninCoordinator alloc] initWithBaseViewController:viewController
                                                        browser:browser
                                                       identity:identity
                                                    accessPoint:accessPoint
                                                    promoAction:promoAction];
}

+ (instancetype)
    firstRunCoordinatorWithBaseViewController:(UIViewController*)viewController
                                      browser:(Browser*)browser
                                syncPresenter:(id<SyncPresenter>)syncPresenter {
  return [[UserSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                        identity:nil
                     accessPoint:AccessPoint::ACCESS_POINT_START_PAGE
                     promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO];
}

+ (instancetype)
    upgradeSigninPromoCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                browser:(Browser*)browser {
  return [[UserSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                        identity:nil
                     accessPoint:AccessPoint::ACCESS_POINT_SIGNIN_PROMO
                     promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO];
}

+ (instancetype)
    advancedSettingsSigninCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                                    browser:(Browser*)browser {
  return [[AdvancedSettingsSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser];
}

+ (instancetype)addAccountCoordinatorWithBaseViewController:
                    (UIViewController*)viewController
                                                    browser:(Browser*)browser
                                                accessPoint:
                                                    (AccessPoint)accessPoint {
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint
                     promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO];
}

+ (instancetype)
    reAuthenticationCoordinatorWithBaseViewController:
        (UIViewController*)viewController
                                              browser:(Browser*)browser

                                          accessPoint:(AccessPoint)accessPoint
                                          promoAction:(PromoAction)promoAction {
  return [[AddAccountSigninCoordinator alloc]
      initWithBaseViewController:viewController
                         browser:browser
                     accessPoint:accessPoint
                     promoAction:PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO];
}

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  // This method needs to be implemented in the subclass.
  NOTREACHED();
}

@end
