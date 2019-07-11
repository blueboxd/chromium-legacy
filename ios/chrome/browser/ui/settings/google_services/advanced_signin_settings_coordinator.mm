// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/google_services/advanced_signin_settings_coordinator.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/driver/sync_service.h"
#include "components/unified_consent/unified_consent_metrics.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/signin/authentication_service.h"
#import "ios/chrome/browser/signin/authentication_service_factory.h"
#include "ios/chrome/browser/sync/profile_sync_service_factory.h"
#include "ios/chrome/browser/sync/sync_setup_service.h"
#include "ios/chrome/browser/sync/sync_setup_service_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/settings/google_services/advanced_signin_settings_navigation_controller.h"
#import "ios/chrome/browser/ui/settings/google_services/google_services_settings_coordinator.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using l10n_util::GetNSString;

NSString* const kSyncSettingsConfirmButtonId =
    @"kAdvancedSyncSettingsConfirmButtonId";
NSString* const kSyncSettingsCancelButtonId =
    @"kAdvancedSyncSettingsCancelButtonId";

// Advanced sign-in settings result.
typedef NS_ENUM(NSInteger, AdvancedSigninSettingsCoordinatorResult) {
  // The user confirmed the advanced sync settings.
  AdvancedSyncSettingsCoordinatorResultConfirm,
  // The user canceled the advanced sync settings.
  AdvancedSigninSettingsCoordinatorResultCancel,
  // Chrome aborted the advanced sync settings.
  AdvancedSigninSettingsCoordinatorResultInterrupted,
};

@interface AdvancedSigninSettingsCoordinator ()

// Google services settings coordinator.
@property(nonatomic, strong)
    GoogleServicesSettingsCoordinator* googleServicesSettingsCoordinator;
// Advanced signin settings navigation controller.
@property(nonatomic, strong) AdvancedSigninSettingsNavigationController*
    advancedSigninSettingsNavigationController;
// Confirm cancel sign-in/sync dialog.
@property(nonatomic, strong)
    AlertCoordinator* cancelConfirmationAlertCoordinator;

@end

@implementation AdvancedSigninSettingsCoordinator

- (void)start {
  self.advancedSigninSettingsNavigationController =
      [[AdvancedSigninSettingsNavigationController alloc] init];
  self.advancedSigninSettingsNavigationController.modalPresentationStyle =
      UIModalPresentationFormSheet;
  self.googleServicesSettingsCoordinator = [[GoogleServicesSettingsCoordinator
      alloc]
      initWithBaseViewController:self.advancedSigninSettingsNavigationController
                    browserState:self.browserState
                            mode:
                              GoogleServicesSettingsModeAdvancedSigninSettings];
  self.googleServicesSettingsCoordinator.dispatcher = self.dispatcher;
  self.googleServicesSettingsCoordinator.navigationController =
      self.advancedSigninSettingsNavigationController;
  [self.googleServicesSettingsCoordinator start];
  self.googleServicesSettingsCoordinator.viewController.navigationItem
      .leftBarButtonItem = [self navigationCancelButton];
  self.googleServicesSettingsCoordinator.viewController.navigationItem
      .rightBarButtonItem = [self navigationConfirmButton];
  [self.baseViewController
      presentViewController:self.advancedSigninSettingsNavigationController
                   animated:YES
                 completion:nil];
}

- (void)abortWithDismiss:(BOOL)dismiss
                animated:(BOOL)animated
              completion:(ProceduralBlock)completion {
  if (!self.advancedSigninSettingsNavigationController) {
    if (completion) {
      completion();
    }
    return;
  }
  DCHECK_EQ(self.advancedSigninSettingsNavigationController,
            self.baseViewController.presentedViewController);
  if (dismiss) {
    [self dismissViewControllerAndFinishWithResult:
              AdvancedSigninSettingsCoordinatorResultInterrupted
                                          animated:animated
                                        completion:completion];
  } else {
    [self
        finishedWithResult:AdvancedSigninSettingsCoordinatorResultInterrupted];
    if (completion) {
      completion();
    }
  }
}

#pragma mark - Private

// Called once the view controller has been removed (if needed).
// |result|, YES if the user accepts to sync.
- (void)finishedWithResult:(AdvancedSigninSettingsCoordinatorResult)result {
  DCHECK(self.advancedSigninSettingsNavigationController);
  SyncSetupService* syncSetupService =
      SyncSetupServiceFactory::GetForBrowserState(self.browserState);
  switch (result) {
    case AdvancedSyncSettingsCoordinatorResultConfirm: {
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_ConfirmAdvancedSyncSettings"));
      syncer::SyncService* syncService =
          ProfileSyncServiceFactory::GetForBrowserState(self.browserState);
      unified_consent::metrics::RecordSyncSetupDataTypesHistrogam(
          syncService->GetUserSettings(), self.browserState->GetPrefs());
      if (syncSetupService->IsSyncEnabled()) {
        // FirstSetupComplete flag should be only turned on when the user agrees
        // to start Sync.
        syncSetupService->PrepareForFirstSyncSetup();
        syncSetupService->SetFirstSetupComplete();
      }
      break;
    }
    case AdvancedSigninSettingsCoordinatorResultCancel:
      base::RecordAction(base::UserMetricsAction(
          "Signin_Signin_ConfirmCancelAdvancedSyncSettings"));
      syncSetupService->CommitSyncChanges();
      AuthenticationServiceFactory::GetForBrowserState(self.browserState)
          ->SignOut(signin_metrics::ABORT_SIGNIN, nil);
      break;
    case AdvancedSigninSettingsCoordinatorResultInterrupted:
      base::RecordAction(
          base::UserMetricsAction("Signin_Signin_AbortAdvancedSyncSettings"));
      break;
  }
  [self.googleServicesSettingsCoordinator stop];
  self.googleServicesSettingsCoordinator.delegate = nil;
  self.googleServicesSettingsCoordinator = nil;
  DCHECK(!syncSetupService->HasUncommittedChanges())
      << "-[GoogleServicesSettingsCoordinator stop] should commit sync "
         "changes.";
  BOOL signedin = result != AdvancedSigninSettingsCoordinatorResultCancel;
  [self.delegate advancedSigninSettingsCoordinatorDidClose:self
                                                  signedin:signedin];
  self.advancedSigninSettingsNavigationController = nil;
}

// This method should be moved to the view controller.
- (UIBarButtonItem*)navigationCancelButton {
  UIBarButtonItem* cancelButton = [[UIBarButtonItem alloc]
      initWithBarButtonSystemItem:UIBarButtonSystemItemCancel
                           target:self
                           action:@selector(navigationCancelButtonAction)];
  cancelButton.accessibilityIdentifier = kSyncSettingsCancelButtonId;
  return cancelButton;
}

// This method should be moved to the view controller.
- (UIBarButtonItem*)navigationConfirmButton {
  UIBarButtonItem* confirmButton = [[UIBarButtonItem alloc]
      initWithTitle:GetNSString(
                        IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CONFIRM_MAIN_BUTTON)
              style:UIBarButtonItemStyleDone
             target:self
             action:@selector(navigationConfirmButtonAction)];
  confirmButton.accessibilityIdentifier = kSyncSettingsConfirmButtonId;
  return confirmButton;
}

// Called by the cancel button from the navigation controller. Presents a
// UIAlert to ask the user if wants to cancel the sign-in.
- (void)navigationCancelButtonAction {
  base::RecordAction(
      base::UserMetricsAction("Signin_Signin_CancelAdvancedSyncSettings"));
  self.cancelConfirmationAlertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.advancedSigninSettingsNavigationController
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
                  [weakSelf alertButtonActionWithCancelSync:NO];
                }
                 style:UIAlertActionStyleCancel];
  [self.cancelConfirmationAlertCoordinator
      addItemWithTitle:
          GetNSString(
              IDS_IOS_ADVANCED_SIGNIN_SETTINGS_CANCEL_SYNC_ALERT_CANCEL_SYNC_BUTTON)
                action:^{
                  [weakSelf alertButtonActionWithCancelSync:YES];
                }
                 style:UIAlertActionStyleDefault];
  [self.cancelConfirmationAlertCoordinator start];
}

// Called by the confirm button from tne navigation controller. Validates the
// sync preferences chosen by the user, starts the sync, close the completion
// callback and closes the advanced sign-in settings.
- (void)navigationConfirmButtonAction {
  DCHECK_EQ(self.advancedSigninSettingsNavigationController,
            self.baseViewController.presentedViewController);
  [self dismissViewControllerAndFinishWithResult:
            AdvancedSyncSettingsCoordinatorResultConfirm
                                        animated:YES
                                      completion:nil];
}

// Called when a button of |self.cancelConfirmationAlertCoordinator| is pressed.
- (void)alertButtonActionWithCancelSync:(BOOL)cancelSync {
  DCHECK(self.cancelConfirmationAlertCoordinator);
  [self.cancelConfirmationAlertCoordinator stop];
  self.cancelConfirmationAlertCoordinator = nil;
  if (cancelSync) {
    [self dismissViewControllerAndFinishWithResult:
              AdvancedSigninSettingsCoordinatorResultCancel
                                          animated:YES
                                        completion:nil];
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Signin_Signin_CancelCancelAdvancedSyncSettings"));
  }
}

// Dismisses the current view controller with animation, and calls
// -[self finishedWithResult:] with |result|.
- (void)dismissViewControllerAndFinishWithResult:
            (AdvancedSigninSettingsCoordinatorResult)result
                                        animated:(BOOL)animated
                                      completion:(ProceduralBlock)completion {
  DCHECK_EQ(self.advancedSigninSettingsNavigationController,
            self.baseViewController.presentedViewController);
  __weak __typeof(self) weakSelf = self;
  ProceduralBlock dismissCompletion = ^() {
    [weakSelf finishedWithResult:result];
    if (completion) {
      completion();
    }
  };
  [self.baseViewController dismissViewControllerAnimated:animated
                                              completion:dismissCompletion];
}

@end
