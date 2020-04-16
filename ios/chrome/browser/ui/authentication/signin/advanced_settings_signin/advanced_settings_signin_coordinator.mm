// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_coordinator.h"

#import "base/metrics/user_metrics.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#import "ios/chrome/browser/sync/profile_sync_service_factory.h"
#import "ios/chrome/browser/sync/sync_setup_service.h"
#import "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_mediator.h"
#import "ios/chrome/browser/ui/authentication/signin/advanced_settings_signin/advanced_settings_signin_navigation_controller.h"
#import "ios/chrome/browser/ui/authentication/signin/signin_coordinator+protected.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_mode.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::RecordAction;
using base::UserMetricsAction;
using l10n_util::GetNSString;

@interface AdvancedSettingsSigninCoordinator () <
    AdvancedSettingsSigninNavigationControllerNavigationDelegate,
    UIAdaptivePresentationControllerDelegate>

// Advanced settings sign-in mediator.
@property(nonatomic, strong)
    AdvancedSettingsSigninMediator* advancedSettingsSigninMediator;
// View controller presented by this coordinator.
@property(nonatomic, strong) AdvancedSettingsSigninNavigationController*
    advancedSettingsSigninNavigationController;
// Google services settings coordinator.
@property(nonatomic, strong)
    GoogleServicesSettingsCoordinator* googleServicesSettingsCoordinator;
// Confirm cancel sign-in/sync dialog.
@property(nonatomic, strong)
    AlertCoordinator* cancelConfirmationAlertCoordinator;

@end

@implementation AdvancedSettingsSigninCoordinator

#pragma mark - SigninCoordinator

- (void)start {
  [super start];
  self.advancedSettingsSigninNavigationController =
      [[AdvancedSettingsSigninNavigationController alloc] init];
  self.advancedSettingsSigninNavigationController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  self.advancedSettingsSigninNavigationController.navigationDelegate = self;

  // Init and start Google settings coordinator.
  GoogleServicesSettingsMode mode =
      GoogleServicesSettingsModeAdvancedSigninSettings;
  self.googleServicesSettingsCoordinator =
      [[GoogleServicesSettingsCoordinator alloc]
          initWithBaseNavigationController:
              self.advancedSettingsSigninNavigationController
                                   browser:self.browser
                                      mode:mode];
  [self.googleServicesSettingsCoordinator start];

  // Create the mediator.
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  AuthenticationService* authenticationService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  syncer::SyncService* syncService =
      ProfileSyncServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  self.advancedSettingsSigninMediator = [[AdvancedSettingsSigninMediator alloc]
      initWithSyncSetupService:syncSetupService
         authenticationService:authenticationService
                   syncService:syncService
                   prefService:self.browser->GetBrowserState()->GetPrefs()];
  self.advancedSettingsSigninNavigationController.presentationController
      .delegate = self;

  // Present the navigation controller that now contains the Google settings
  // view controller.
  [self.baseViewController
      presentViewController:self.advancedSettingsSigninNavigationController
                   animated:YES
                 completion:nil];
}

- (void)interruptWithAction:(SigninCoordinatorInterruptAction)action
                 completion:(ProceduralBlock)completion {
  DCHECK(self.advancedSettingsSigninNavigationController);
  [self.googleServicesSettingsCoordinator stop];
  self.googleServicesSettingsCoordinator = nil;
  switch (action) {
    case SigninCoordinatorInterruptActionNoDismiss:
      [self finishedWithSigninResult:SigninCoordinatorResultInterrupted];
      if (completion) {
        completion();
      }
      break;
    case SigninCoordinatorInterruptActionDismissWithoutAnimation:
      [self dismissViewControllerAndFinishWithResult:
                SigninCoordinatorResultInterrupted
                                            animated:NO
                                          completion:completion];
      break;
    case SigninCoordinatorInterruptActionDismissWithAnimation:
      [self dismissViewControllerAndFinishWithResult:
                SigninCoordinatorResultInterrupted
                                            animated:YES
                                          completion:completion];
      break;
  }
}

- (BOOL)isSettingsViewPresented {
  // TODO(crbug.com/971989): Remove this method.
  return YES;
}

#pragma mark - Private

// Called when a button of |self.cancelConfirmationAlertCoordinator| is pressed.
- (void)cancelConfirmationWithShouldCancelSignin:(BOOL)shouldCancelSignin {
  DCHECK(self.cancelConfirmationAlertCoordinator);
  [self.cancelConfirmationAlertCoordinator stop];
  self.cancelConfirmationAlertCoordinator = nil;
  if (shouldCancelSignin) {
    [self dismissViewControllerAndFinishWithResult:
              SigninCoordinatorResultCanceledByUser
                                          animated:YES
                                        completion:nil];
  } else {
    RecordAction(
        UserMetricsAction("Signin_Signin_CancelCancelAdvancedSyncSettings"));
  }
}

// Dismisses the current view controller with |animated|, triggers the
// coordinator cleanup and then calls |completion|.
- (void)dismissViewControllerAndFinishWithResult:(SigninCoordinatorResult)result
                                        animated:(BOOL)animated
                                      completion:(ProceduralBlock)completion {
  DCHECK_EQ(self.advancedSettingsSigninNavigationController,
            self.baseViewController.presentedViewController);
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock dismissCompletion = ^() {
    [weakSelf finishedWithSigninResult:result];
    if (completion) {
      completion();
    }
  };
  [self.baseViewController dismissViewControllerAnimated:animated
                                              completion:dismissCompletion];
}

// Does the cleanup once the view has been dismissed, calls the metrics and
// calls |runCompletionCallbackWithSigninResult:identity:| to finish the
// sign-in.
- (void)finishedWithSigninResult:(SigninCoordinatorResult)signinResult {
  DCHECK(self.advancedSettingsSigninNavigationController);
  DCHECK(self.advancedSettingsSigninMediator);
  [self.advancedSettingsSigninMediator
      saveUserPreferenceForSigninResult:signinResult];
  self.advancedSettingsSigninNavigationController = nil;
  self.advancedSettingsSigninMediator = nil;
  [self.googleServicesSettingsCoordinator stop];
  self.googleServicesSettingsCoordinator = nil;
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  DCHECK(!syncSetupService->HasUncommittedChanges())
      << "-[GoogleServicesSettingsCoordinator stop] should commit sync "
         "changes.";
  AuthenticationService* authService =
      AuthenticationServiceFactory::GetForBrowserState(
          self.browser->GetBrowserState());
  ChromeIdentity* identity = authService->GetAuthenticatedIdentity();
  [self runCompletionCallbackWithSigninResult:signinResult
                                     identity:identity
                   showAdvancedSettingsSignin:NO];
}

- (void)showCancelConfirmationAlert {
  DCHECK(!self.cancelConfirmationAlertCoordinator);
  RecordAction(UserMetricsAction("Signin_Signin_CancelAdvancedSyncSettings"));
  self.cancelConfirmationAlertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.advancedSettingsSigninNavigationController
                         browser:self.browser
                           title:
                               GetNSString(
                                   IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_TITLE)
                         message:
                             GetNSString(
                                 IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_MESSAGE)];
  __weak __typeof(self) weakSelf = self;
  [self.cancelConfirmationAlertCoordinator
      addItemWithTitle:
          GetNSString(
              IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_BACK_BUTTON)
                action:^{
                  [weakSelf cancelConfirmationWithShouldCancelSignin:NO];
                }
                 style:UIAlertActionStyleCancel];
  [self.cancelConfirmationAlertCoordinator
      addItemWithTitle:
          GetNSString(
              IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_CANCEL_SYNC_BUTTON)
                action:^{
                  [weakSelf cancelConfirmationWithShouldCancelSignin:YES];
                }
                 style:UIAlertActionStyleDefault];
  [self.cancelConfirmationAlertCoordinator start];
}

#pragma mark - AdvancedSettingsSigninNavigationControllerNavigationDelegate

- (void)navigationCancelButtonWasTapped {
  [self showCancelConfirmationAlert];
}

- (void)navigationConfirmButtonWasTapped {
  DCHECK(!self.cancelConfirmationAlertCoordinator);
  [self dismissViewControllerAndFinishWithResult:SigninCoordinatorResultSuccess
                                        animated:YES
                                      completion:nil];
}

#pragma mark - UIAdaptivePresentationControllerDelegate

- (BOOL)presentationControllerShouldDismiss:
    (UIPresentationController*)presentationController {
  return NO;
}

- (void)presentationControllerDidAttemptToDismiss:
    (UIPresentationController*)presentationController {
  [self showCancelConfirmationAlert];
}

@end
