
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface UserSigninCoordinator () <UnifiedConsentCoordinatorDelegate>

// Coordinator that handles the user consent before the user signs in.
@property(nonatomic, strong)
    UnifiedConsentCoordinator* unifiedConsentCoordinator;
// View controller that handles the sign-in UI.
@property(nonatomic, strong) UserSigninViewController* viewController;
// Suggested identity shown at sign-in.
@property(nonatomic, strong) ChromeIdentity* defaultIdentity;
// View where the sign-in button was displayed.
@property(nonatomic, assign) AccessPoint accessPoint;
// Promo button used to trigger the sign-in.
@property(nonatomic, assign) PromoAction promoAction;

@end

@implementation UserSigninCoordinator

#pragma mark - Public

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                                  identity:(ChromeIdentity*)identity
                               accessPoint:(AccessPoint)accessPoint
                               promoAction:(PromoAction)promoAction {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _defaultIdentity = identity;
    _accessPoint = accessPoint;
    _promoAction = promoAction;
  }
  return self;
}

#pragma mark - ChromeCoordinator

- (void)start {
  [super start];
  self.viewController = [[UserSigninViewController alloc] init];

  self.unifiedConsentCoordinator = [[UnifiedConsentCoordinator alloc] init];
  self.unifiedConsentCoordinator.delegate = self;

  // Set UnifiedConsentCoordinator properties.
  self.unifiedConsentCoordinator.selectedIdentity = self.defaultIdentity;
  self.unifiedConsentCoordinator.autoOpenIdentityPicker =
      self.promoAction == PromoAction::PROMO_ACTION_NOT_DEFAULT;

  [self.unifiedConsentCoordinator start];

  // Display UnifiedConsentViewController within the host.
  self.viewController.unifiedConsentViewController =
      self.unifiedConsentCoordinator.viewController;
  [self.baseViewController presentViewController:self.viewController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [super stop];
  self.unifiedConsentCoordinator = nil;
}

#pragma mark - UnifiedConsentCoordinatorDelegate

- (void)unifiedConsentCoordinatorDidTapSettingsLink:
    (UnifiedConsentCoordinator*)coordinator {
  // TODO(crbug.com/971989): Needs implementation.
}

- (void)unifiedConsentCoordinatorDidReachBottom:
    (UnifiedConsentCoordinator*)coordinator {
  // TODO(crbug.com/971989): Needs implementation.
}

- (void)unifiedConsentCoordinatorDidTapOnAddAccount:
    (UnifiedConsentCoordinator*)coordinator {
  // TODO(crbug.com/971989): Needs implementation.
}

- (void)unifiedConsentCoordinatorNeedPrimaryButtonUpdate:
    (UnifiedConsentCoordinator*)coordinator {
  // TODO(crbug.com/971989): Needs implementation.
}

@end
