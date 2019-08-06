// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_coordinator.h"

#include "components/autofill/core/browser/personal_data_manager.h"
#import "components/autofill/ios/browser/personal_data_manager_observer_bridge.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/ui/alert_coordinator/alert_coordinator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator_delegate.h"
#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_view_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AutofillAddCreditCardCoordinator () <AddCreditCardMediatorDelegate>

// Displays message for invalid credit card data.
@property(nonatomic, strong) AlertCoordinator* alertCoordinator;

// The view controller attached to this coordinator.
@property(nonatomic, strong)
    AutofillAddCreditCardViewController* addCreditCardViewController;

// The mediator for the view controller attatched to this coordinator.
@property(nonatomic, strong) AutofillAddCreditCardMediator* mediator;

@end

@implementation AutofillAddCreditCardCoordinator

- (void)start {
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          self.browserState);

  self.mediator = [[AutofillAddCreditCardMediator alloc]
         initWithDelegate:self
      personalDataManager:personalDataManager];

  self.addCreditCardViewController =
      [[AutofillAddCreditCardViewController alloc]
          initWithDelegate:self.mediator];

  UINavigationController* navigationController = [[UINavigationController alloc]
      initWithRootViewController:self.addCreditCardViewController];

  [self.baseViewController presentViewController:navigationController
                                        animated:YES
                                      completion:nil];
}

- (void)stop {
  [self.addCreditCardViewController.navigationController
      dismissViewControllerAnimated:YES
                         completion:nil];
  self.addCreditCardViewController = nil;
  self.mediator = nil;
}

#pragma mark - AddCreditCardMediatorDelegate

- (void)creditCardMediatorDidFinish:(AutofillAddCreditCardMediator*)mediator {
  [self stop];
}

- (void)creditCardMediatorHasInvalidCardNumber:
    (AutofillAddCreditCardMediator*)mediator {
  [self showAlertWithMessage:
            l10n_util::GetNSString(
                IDS_IOS_ADD_CREDIT_CARD_INVALID_CARD_NUMBER_ALERT)];
}

- (void)creditCardMediatorHasInvalidExpirationDate:
    (AutofillAddCreditCardMediator*)mediator {
  [self showAlertWithMessage:
            l10n_util::GetNSString(
                IDS_IOS_ADD_CREDIT_CARD_INVALID_EXPIRATION_DATE_ALERT)];
}

#pragma mark - Helper Methods

// Shows alert with received message by |AlertCoordinator|.
- (void)showAlertWithMessage:(NSString*)message {
  self.alertCoordinator = [[AlertCoordinator alloc]
      initWithBaseViewController:self.addCreditCardViewController
                           title:message
                         message:nil];

  [self.alertCoordinator start];
}

@end
