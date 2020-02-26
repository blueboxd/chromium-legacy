
// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/user_signin/user_signin_view_controller.h"
#import "ios/chrome/browser/ui/authentication/unified_consent/unified_consent_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface UserSigninCoordinator () <UnifiedConsentCoordinatorDelegate,
                                     UserSigninViewControllerDelegate,
                                     UserSigninMediatorDelegate>

// Coordinator that handles the user consent before the user signs in.
@property(nonatomic, strong)
    UnifiedConsentCoordinator* unifiedConsentCoordinator;
// Coordinator that handles adding a user account.
@property(nonatomic, strong) SigninCoordinator* addAccountSigninCoordinator;
// View controller that handles the sign-in UI.
@property(nonatomic, strong) UserSigninViewController* viewController;
// Mediator that handles the sign-in authentication state.
@property(nonatomic, strong) UserSigninMediator* mediator;
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

#pragma mark - SigninCoordinator

- (void)start {
  [super start];
  self.viewController = [[UserSigninViewController alloc] init];
  self.viewController.delegate = self;

  self.mediator = [[UserSigninMediator alloc]
      initWithAuthenticationService:AuthenticationServiceFactory::
                                        GetForBrowserState(self.browserState)];
  self.mediator.delegate = self;

  self.unifiedConsentCoordinator = [[UnifiedConsentCoordinator alloc]
      initWithBaseViewController:nil
                         browser:self.browser];
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
  DCHECK_EQ(self.unifiedConsentCoordinator, coordinator);
  [self.viewController markUnifiedConsentScreenReachedBottom];
}

- (void)unifiedConsentCoordinatorDidTapOnAddAccount:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(self.unifiedConsentCoordinator, coordinator);
  [self userSigninViewControllerDidTapOnAddAccount];
}

- (void)unifiedConsentCoordinatorNeedPrimaryButtonUpdate:
    (UnifiedConsentCoordinator*)coordinator {
  DCHECK_EQ(self.unifiedConsentCoordinator, coordinator);
  [self userSigninMediatorNeedPrimaryButtonUpdate];
}

#pragma mark - UserSigninViewControllerDelegate

- (BOOL)unifiedConsentCoordinatorHasIdentity {
  return self.unifiedConsentCoordinator.selectedIdentity != nil;
}

- (void)userSigninViewControllerDidTapOnAddAccount {
  self.addAccountSigninCoordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:self.viewController
                                          browser:self.browser
                                      accessPoint:self.accessPoint];

  __weak UserSigninCoordinator* weakSelf = self;
  self.addAccountSigninCoordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult, ChromeIdentity* identity) {
        if (signinResult == SigninCoordinatorResultSuccess) {
          weakSelf.unifiedConsentCoordinator.selectedIdentity = identity;
          [weakSelf.addAccountSigninCoordinator stop];
          weakSelf.addAccountSigninCoordinator = nil;
        }
      };
  [self.addAccountSigninCoordinator start];
}

- (void)userSigninViewControllerDidScrollOnUnifiedConsent {
  [self.unifiedConsentCoordinator scrollToBottom];
}

- (void)userSigninViewControllerDidTapOnSkipSignin {
  [self.mediator cancelSignin];
}

#pragma mark - UserSigninMediatorDelegate

- (void)userSigninMediatorSigninFinishedWithResult:
    (SigninCoordinatorResult)signinResult {
  [self recordSigninMetricsWithResult:signinResult];

  __weak UserSigninCoordinator* weakSelf = self;
  ProceduralBlock completion = ^void() {
    [weakSelf
        runCompletionCallbackWithSigninResult:signinResult
                                     identity:weakSelf.unifiedConsentCoordinator
                                                  .selectedIdentity];
  };

  [self.viewController dismissViewControllerAnimated:YES completion:completion];

  self.unifiedConsentCoordinator.delegate = nil;
  self.unifiedConsentCoordinator = nil;
}

- (void)userSigninMediatorNeedPrimaryButtonUpdate {
  [self.viewController updatePrimaryButtonStyle];
}

- (void)recordSigninMetricsWithResult:(SigninCoordinatorResult)signinResult {
  switch (signinResult) {
    case SigninCoordinatorResultSuccess: {
      signin_metrics::LogSigninAccessPointCompleted(self.accessPoint,
                                                    self.promoAction);
      break;
    }
    case SigninCoordinatorResultCanceledByUser: {
      base::RecordAction(base::UserMetricsAction("Signin_Undo_Signin"));
      break;
    }
    case SigninCoordinatorResultInterrupted: {
      // TODO(crbug.com/951145): Add metric when the sign-in has been
      // interrupted.
      break;
    }
  }
}

@end
