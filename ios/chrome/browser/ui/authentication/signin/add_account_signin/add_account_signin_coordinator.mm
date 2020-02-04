// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/add_account_signin/add_account_signin_coordinator.h"

#include "components/signin/public/base/signin_metrics.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/signin_util.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_interaction_manager.h"
#import "ios/public/provider/chrome/browser/signin/chrome_identity_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using signin_metrics::AccessPoint;
using signin_metrics::PromoAction;

@interface AddAccountSigninCoordinator () <
    ChromeIdentityInteractionManagerDelegate>

// Coordinator to display modal alerts to the user.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// Manager that handles interactions to add identities.
@property(nonatomic, strong)
    ChromeIdentityInteractionManager* identityInteractionManager;

@end

@implementation AddAccountSigninCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                               accessPoint:(AccessPoint)accessPoint
                               promoAction:(PromoAction)promoAction {
  return [super initWithBaseViewController:viewController browser:browser];
}

#pragma mark - ChromeCoordinator

- (void)start {
  self.identityInteractionManager =
      ios::GetChromeBrowserProvider()
          ->GetChromeIdentityService()
          ->CreateChromeIdentityInteractionManager(
              self.browser->GetBrowserState(), self);

  __weak AddAccountSigninCoordinator* weakSelf = self;
  [self.identityInteractionManager
      addAccountWithCompletion:^(ChromeIdentity* identity, NSError* error) {
        [weakSelf completeAddAccountFlow:identity error:error];
      }];
}

- (void)stop {
  [self.identityInteractionManager cancelAndDismissAnimated:NO];
  [self.alertCoordinator executeCancelHandler];
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

#pragma mark - ChromeIdentityInteractionManagerDelegate

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
    dismissViewControllerAnimated:(BOOL)animated
                       completion:(ProceduralBlock)completion {
  [self.baseViewController.presentedViewController
      dismissViewControllerAnimated:animated
                         completion:completion];
}

- (void)interactionManager:(ChromeIdentityInteractionManager*)interactionManager
     presentViewController:(UIViewController*)viewController
                  animated:(BOOL)animated
                completion:(ProceduralBlock)completion {
  [self.baseViewController presentViewController:viewController
                                        animated:animated
                                      completion:completion];
}

#pragma mark - Utility methods

// Completes the add account flow including handling any errors that have not
// been handled internally by ChromeIdentity.
// |identity| is the identity of the added account.
// |error| is an error reported by the SSOAuth following adding an account.
- (void)completeAddAccountFlow:(ChromeIdentity*)identity error:(NSError*)error {
  if (!self.identityInteractionManager) {
    return;
  }

  if (error) {
    // Filter out errors handled internally by ChromeIdentity.
    if (!ShouldHandleSigninError(error)) {
      [self runCompletionCallbackWithSigninResult:
                SigninCoordinatorResultCanceledByUser
                                         identity:identity];
      return;
    }

    __weak AddAccountSigninCoordinator* weakSelf = self;
    ProceduralBlock dismissAction = ^{
      [weakSelf runCompletionCallbackWithSigninResult:
                    SigninCoordinatorResultCanceledByUser
                                             identity:identity];
    };
    self.alertCoordinator =
        ErrorCoordinator(error, dismissAction, self.baseViewController);
    [self.alertCoordinator start];
    return;
  }

  [self runCompletionCallbackWithSigninResult:SigninCoordinatorResultSuccess
                                     identity:identity];
}

// Clears the state of this coordinator following the completion of the add
// account flow. |signinResult| is the state of sign-in at add account flow
// completion. |identity| is the identity of the added account.
- (void)runCompletionCallbackWithSigninResult:
            (SigninCoordinatorResult)signinResult
                                     identity:(ChromeIdentity*)identity {
  // Cleaning up and calling the |signinCompletion| should be done last.
  self.identityInteractionManager = nil;
  self.alertCoordinator = nil;

  if (self.signinCompletion) {
    self.signinCompletion(signinResult, identity);
    self.signinCompletion = nil;
  }
}

@end
