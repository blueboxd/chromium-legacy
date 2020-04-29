// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_coordinator.h"

#import "base/ios/block_types.h"
#include "base/logging.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/authentication_ui_util.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_controller.h"
#import "ios/chrome/browser/ui/signin_interaction/signin_interaction_presenting.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SigninInteractionCoordinator () <SigninInteractionPresenting>

// Coordinator to present alerts.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// The controller managed by this coordinator.
@property(nonatomic, strong) SigninInteractionController* controller;

// The coordinator used to control sign-in UI flows.
// See https://crbug.com/971989 for the migration plan to exclusively use
// SigninCoordinator to trigger sign-in UI.
@property(nonatomic, strong) SigninCoordinator* coordinator;

// The UIViewController upon which UI should be presented.
@property(nonatomic, strong) UIViewController* presentingViewController;

// Bookkeeping for the top-most view controller.
@property(nonatomic, strong) UIViewController* topViewController;

// Sign-in completion.
@property(nonatomic, copy) signin_ui::CompletionCallback signinCompletion;

@end

@implementation SigninInteractionCoordinator

- (instancetype)initWithBrowser:(Browser*)browser {
  DCHECK(browser);
  self = [super initWithBaseViewController:nil browser:browser];
  return self;
}

- (void)signInWithIdentity:(ChromeIdentity*)identity
                 accessPoint:(signin_metrics::AccessPoint)accessPoint
                 promoAction:(signin_metrics::PromoAction)promoAction
    presentingViewController:(UIViewController*)viewController
                  completion:(signin_ui::CompletionCallback)completion {
  // Ensure that nothing is done if a sign in operation is already in progress.
  if (self.coordinator || self.controller) {
    return;
  }

  if (base::FeatureList::IsEnabled(kNewSigninArchitecture)) {
    self.coordinator = [SigninCoordinator
        userSigninCoordinatorWithBaseViewController:viewController
                                            browser:self.browser
                                           identity:identity
                                        accessPoint:accessPoint
                                        promoAction:promoAction];

    __weak SigninInteractionCoordinator* weakSelf = self;
    self.coordinator.signinCompletion =
        ^(SigninCoordinatorResult signinResult, SigninCompletionInfo*) {
          if (completion) {
            completion(signinResult == SigninCoordinatorResultSuccess);
          }
          [weakSelf.coordinator stop];
          weakSelf.coordinator = nil;
        };

    [self.coordinator start];
  } else {
    [self setupForSigninOperationWithAccessPoint:accessPoint
                                     promoAction:promoAction
                        presentingViewController:viewController
                                      completion:completion];

    [self.controller signInWithIdentity:identity
                             completion:[self callbackToClearState]];
  }
}

- (void)reAuthenticateWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                          promoAction:(signin_metrics::PromoAction)promoAction
             presentingViewController:(UIViewController*)viewController
                           completion:
                               (signin_ui::CompletionCallback)completion {
  // Ensure that nothing is done if a sign in operation is already in progress.
  if (self.coordinator || self.controller) {
    return;
  }

  if (base::FeatureList::IsEnabled(kNewSigninArchitecture)) {
    self.coordinator = [SigninCoordinator
        reAuthenticationCoordinatorWithBaseViewController:viewController
                                                  browser:self.browser
                                              accessPoint:accessPoint
                                              promoAction:promoAction];

    __weak SigninInteractionCoordinator* weakSelf = self;
    self.coordinator.signinCompletion =
        ^(SigninCoordinatorResult signinResult, SigninCompletionInfo*) {
          if (completion) {
            completion(signinResult == SigninCoordinatorResultSuccess);
          }
          [weakSelf.coordinator stop];
          weakSelf.coordinator = nil;
        };

    [self.coordinator start];
  } else {
    [self setupForSigninOperationWithAccessPoint:accessPoint
                                     promoAction:promoAction
                        presentingViewController:viewController
                                      completion:completion];

    [self.controller reAuthenticateWithCompletion:[self callbackToClearState]];
  }
}

- (void)addAccountWithAccessPoint:(signin_metrics::AccessPoint)accessPoint
                      promoAction:(signin_metrics::PromoAction)promoAction
         presentingViewController:(UIViewController*)viewController
                       completion:(signin_ui::CompletionCallback)completion {
  // Ensure that nothing is done if a sign in operation is already in progress.
  if (self.coordinator || self.controller) {
    return;
  }

  self.coordinator = [SigninCoordinator
      addAccountCoordinatorWithBaseViewController:viewController
                                          browser:self.browser
                                      accessPoint:accessPoint];

  __weak SigninInteractionCoordinator* weakSelf = self;
  self.coordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult, SigninCompletionInfo*) {
        if (completion) {
          completion(signinResult == SigninCoordinatorResultSuccess);
        }
        [weakSelf.coordinator stop];
        weakSelf.coordinator = nil;
      };

  [self.coordinator start];
}

- (void)showAdvancedSigninSettingsWithPresentingViewController:
    (UIViewController*)viewController {
  self.presentingViewController = viewController;
  [self showAdvancedSigninSettings];
}

- (void)showTrustedVaultReauthenticationWithPresentingViewController:
    (UIViewController*)viewController {
  DCHECK(!self.signinCompletion);
  DCHECK(!self.presentingViewController);
  DCHECK(!self.coordinator);
  self.presentingViewController = viewController;
  self.coordinator = [SigninCoordinator
      trustedVaultReAuthenticationCoordiantorWithBaseViewController:
          viewController
                                                            browser:
                                                                self.browser];
  __weak SigninInteractionCoordinator* weakSelf = self;
  self.coordinator.signinCompletion =
      ^(SigninCoordinatorResult, SigninCompletionInfo*) {
        [weakSelf trustedVaultReauthenticationDone];
      };
  [self.coordinator start];
}

- (void)cancel {
  [self.controller cancel];
  [self interrupSigninCoordinatorWithAction:
            SigninCoordinatorInterruptActionNoDismiss
                                 completion:nil];
}

- (void)cancelAndDismiss {
  [self.controller cancelAndDismiss];
  [self interrupSigninCoordinatorWithAction:
            SigninCoordinatorInterruptActionDismissWithAnimation
                                 completion:nil];
}

- (void)abortAndDismissSettingsViewAnimated:(BOOL)animated
                                 completion:(ProceduralBlock)completion {
  if (self.controller) {
    [self.controller cancel];
    if (completion) {
      completion();
    }
    return;
  }
  SigninCoordinatorInterruptAction action =
      animated ? SigninCoordinatorInterruptActionDismissWithAnimation
               : SigninCoordinatorInterruptActionDismissWithoutAnimation;
  [self interrupSigninCoordinatorWithAction:action completion:completion];
}

#pragma mark - Properties

- (BOOL)isActive {
  return self.controller != nil || self.coordinator != nil;
}

- (BOOL)isSettingsViewPresented {
  return self.coordinator.isSettingsViewPresented;
}

#pragma mark - SigninInteractionPresenting

- (void)presentViewController:(UIViewController*)viewController
                     animated:(BOOL)animated
                   completion:(ProceduralBlock)completion {
  DCHECK_EQ(self.presentingViewController, self.topViewController);
  [self presentTopViewController:viewController
                        animated:animated
                      completion:completion];
}

- (void)presentTopViewController:(UIViewController*)viewController
                        animated:(BOOL)animated
                      completion:(ProceduralBlock)completion {
  DCHECK(viewController);
  DCHECK(self.topViewController);
  [self.topViewController presentViewController:viewController
                                       animated:animated
                                     completion:completion];
  self.topViewController = viewController;
}

- (void)dismissAllViewControllersAnimated:(BOOL)animated
                               completion:(ProceduralBlock)completion {
  DCHECK([self isPresenting]);
  [self.presentingViewController dismissViewControllerAnimated:animated
                                                    completion:completion];
  self.topViewController = self.presentingViewController;
}

- (void)presentError:(NSError*)error
       dismissAction:(ProceduralBlock)dismissAction {
  DCHECK(!self.alertCoordinator);
  DCHECK(self.topViewController);
  DCHECK(![self.topViewController presentedViewController]);
  self.alertCoordinator = ErrorCoordinator(
      error, dismissAction, self.topViewController, self.browser);
  [self.alertCoordinator start];
}

- (void)dismissError {
  [self.alertCoordinator executeCancelHandler];
  [self.alertCoordinator stop];
  self.alertCoordinator = nil;
}

- (BOOL)isPresenting {
  return self.presentingViewController.presentedViewController != nil;
}

#pragma mark - Private Methods

// Sets up relevant instance variables for a sign in operation.
- (void)setupForSigninOperationWithAccessPoint:
            (signin_metrics::AccessPoint)accessPoint
                                   promoAction:
                                       (signin_metrics::PromoAction)promoAction
                      presentingViewController:
                          (UIViewController*)presentingViewController
                                    completion:(signin_ui::CompletionCallback)
                                                   completion {
  DCHECK(![self isPresenting]);
  DCHECK(!self.signinCompletion);
  self.signinCompletion = completion;
  self.presentingViewController = presentingViewController;
  self.topViewController = presentingViewController;

  // TODO(crbug.com/1045047): Use HandlerForProtocol after commands protocol
  // clean up.
  self.controller = [[SigninInteractionController alloc]
           initWithBrowser:self.browser
      presentationProvider:self
               accessPoint:accessPoint
               promoAction:promoAction
                dispatcher:static_cast<
                               id<ApplicationCommands, BrowsingDataCommands>>(
                               self.browser->GetCommandDispatcher())];
}

// Returns a callback that clears the state of the coordinator and runs
// |completion|.
- (SigninInteractionControllerCompletionCallback)callbackToClearState {
  __weak SigninInteractionCoordinator* weakSelf = self;
  SigninInteractionControllerCompletionCallback completionCallback =
      ^(SigninResult signinResult) {
        [weakSelf
            signinInteractionControllerCompletionWithSigninResult:signinResult];
      };
  return completionCallback;
}

// Called when SigninInteractionController is completed.
- (void)signinInteractionControllerCompletionWithSigninResult:
    (SigninResult)signinResult {
  self.controller = nil;
  self.topViewController = nil;
  self.alertCoordinator = nil;
  if (signinResult == SigninResultSignedInnAndOpennSettings) {
    [self showAdvancedSigninSettings];
  } else {
    [self signinDoneWithSuccess:signinResult != SigninResultCanceled];
  }
}

// Shows the advanced sign-in settings UI.
- (void)showAdvancedSigninSettings {
  DCHECK(!self.coordinator);
  DCHECK(self.presentingViewController);
  self.coordinator = [SigninCoordinator
      advancedSettingsSigninCoordinatorWithBaseViewController:
          self.presentingViewController
                                                      browser:self.browser];
  __weak SigninInteractionCoordinator* weakSelf = self;
  self.coordinator.signinCompletion =
      ^(SigninCoordinatorResult signinResult, SigninCompletionInfo*) {
        [weakSelf advancedSigninDoneWithSigninResult:signinResult];
      };
  [self.coordinator start];
}

- (void)advancedSigninDoneWithSigninResult:
    (SigninCoordinatorResult)signinResult {
  [self.coordinator stop];
  self.coordinator = nil;
  [self signinDoneWithSuccess:signinResult == SigninCoordinatorResultSuccess];
}

// Called when the sign-in is done.
- (void)signinDoneWithSuccess:(BOOL)success {
  DCHECK(!self.controller);
  DCHECK(!self.topViewController);
  DCHECK(!self.alertCoordinator);
  self.presentingViewController = nil;
  if (self.signinCompletion) {
    self.signinCompletion(success);
    self.signinCompletion = nil;
  }
}

- (void)interrupSigninCoordinatorWithAction:
            (SigninCoordinatorInterruptAction)action
                                 completion:(ProceduralBlock)completion {
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock interruptCompletion = ^() {
    // |weakSelf.coordinator.signinCompletion| is called before this interrupt
    // block. The signin completion has to set |coordinator| to nil.
    DCHECK(!weakSelf.coordinator);
    if (completion) {
      completion();
    }
  };
  [self.coordinator interruptWithAction:action completion:interruptCompletion];
}

- (void)trustedVaultReauthenticationDone {
  DCHECK(self.coordinator);
  [self.coordinator stop];
  self.coordinator = nil;
  self.presentingViewController = nil;
}

@end
