// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory_coordinator.h"

#include <vector>

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#import "ios/chrome/browser/autofill/form_input_accessory_view_controller.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/password_coordinator.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state/web_state.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryCoordinator () <
    AutofillSecurityAlertPresenter,
    AddressCoordinatorDelegate,
    CardCoordinatorDelegate,
    FormInputAccessoryMediatorDelegate,
    ManualFillAccessoryViewControllerDelegate,
    PasswordCoordinatorDelegate>

// The Mediator for the input accessory view controller.
@property(nonatomic, strong)
    FormInputAccessoryMediator* formInputAccessoryMediator;

// The View Controller for the input accessory view.
@property(nonatomic, strong)
    FormInputAccessoryViewController* formInputAccessoryViewController;

// The object in charge of interacting with the web view. Used to fill the data
// in the forms.
@property(nonatomic, strong)
    ManualFillInjectionHandler* manualFillInjectionHandler;

// The WebStateList for this instance. Used to instantiate the child
// coordinators lazily.
@property(nonatomic, assign) WebStateList* webStateList;

@end

@implementation FormInputAccessoryCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                              browserState:
                                  (ios::ChromeBrowserState*)browserState
                              webStateList:(WebStateList*)webStateList {
  DCHECK(browserState);
  DCHECK(webStateList);
  self = [super initWithBaseViewController:viewController
                              browserState:browserState];
  if (self) {
    _webStateList = webStateList;

    _manualFillInjectionHandler =
        [[ManualFillInjectionHandler alloc] initWithWebStateList:webStateList
                                          securityAlertPresenter:self];

    _formInputAccessoryViewController =
        [[FormInputAccessoryViewController alloc]
            initWithManualFillAccessoryViewControllerDelegate:self];

    auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
        browserState, ServiceAccessType::EXPLICIT_ACCESS);

    // There is no personal data manager in OTR (incognito). Get the original
    // one for manual fallback.
    autofill::PersonalDataManager* personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(
            browserState->GetOriginalChromeBrowserState());

    _formInputAccessoryMediator = [[FormInputAccessoryMediator alloc]
           initWithConsumer:self.formInputAccessoryViewController
                   delegate:self
               webStateList:webStateList
        personalDataManager:personalDataManager
              passwordStore:passwordStore];
  }
  return self;
}

- (void)stop {
  [self stopChildren];
  [self.formInputAccessoryViewController restoreOriginalKeyboardView];
  [self.formInputAccessoryMediator disconnect];
}

#pragma mark - Presenting Children

- (void)stopChildren {
  for (ChromeCoordinator* coordinator in self.childCoordinators) {
    [coordinator stop];
  }
  [self.childCoordinators removeAllObjects];
}

- (void)startPasswordsFromButton:(UIButton*)button {
  DCHECK(self.webStateList->GetActiveWebState());
  const GURL& URL =
      self.webStateList->GetActiveWebState()->GetLastCommittedURL();
  ManualFillPasswordCoordinator* passwordCoordinator =
      [[ManualFillPasswordCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                        browserState:self.browserState
                                 URL:URL
                    injectionHandler:self.manualFillInjectionHandler];
  passwordCoordinator.delegate = self;
  if (IsIPadIdiom()) {
    [passwordCoordinator presentFromButton:button];
  } else {
    [self.formInputAccessoryViewController
        presentView:passwordCoordinator.viewController.view];
  }

  [self.childCoordinators addObject:passwordCoordinator];
}

- (void)startCardsFromButton:(UIButton*)button {
  CardCoordinator* cardCoordinator = [[CardCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                    browserState:self.browserState
                                     ->GetOriginalChromeBrowserState()
                    webStateList:self.webStateList
                injectionHandler:self.manualFillInjectionHandler];
  cardCoordinator.delegate = self;
  if (IsIPadIdiom()) {
    [cardCoordinator presentFromButton:button];
  } else {
    [self.formInputAccessoryViewController
        presentView:cardCoordinator.viewController.view];
  }

  [self.childCoordinators addObject:cardCoordinator];
}

- (void)startAddressFromButton:(UIButton*)button {
  AddressCoordinator* addressCoordinator = [[AddressCoordinator alloc]
      initWithBaseViewController:self.baseViewController
                    browserState:self.browserState
                                     ->GetOriginalChromeBrowserState()
                injectionHandler:self.manualFillInjectionHandler];
  addressCoordinator.delegate = self;
  if (IsIPadIdiom()) {
    [addressCoordinator presentFromButton:button];
  } else {
    [self.formInputAccessoryViewController
        presentView:addressCoordinator.viewController.view];
  }

  [self.childCoordinators addObject:addressCoordinator];
}

#pragma mark - FormInputAccessoryMediatorDelegate

- (void)mediatorDidDetectKeyboardHide:(FormInputAccessoryMediator*)mediator {
  // On iOS 13, beta 3, the popover is not dismissed when the keyboard hides.
  // This explicitly dismiss any popover.
  if (base::ios::IsRunningOnIOS13OrLater() && IsIPadIdiom()) {
    [self stopChildren];
    [self.formInputAccessoryMediator enableSuggestions];
    [self.formInputAccessoryViewController resetManualFallbackIcons];
  }
}

#pragma mark - ManualFillAccessoryViewControllerDelegate

- (void)keyboardButtonPressed {
  [self stopChildren];
  [self.formInputAccessoryMediator enableSuggestions];
  [self.formInputAccessoryViewController unlockManualFallbackView];
}

- (void)accountButtonPressed:(UIButton*)sender {
  [self stopChildren];
  [self startAddressFromButton:sender];
  [self.formInputAccessoryViewController lockManualFallbackView];
  [self.formInputAccessoryMediator disableSuggestions];
}

- (void)cardButtonPressed:(UIButton*)sender {
  [self stopChildren];
  [self startCardsFromButton:sender];
  [self.formInputAccessoryViewController lockManualFallbackView];
  [self.formInputAccessoryMediator disableSuggestions];
}

- (void)passwordButtonPressed:(UIButton*)sender {
  [self stopChildren];
  [self startPasswordsFromButton:sender];
  [self.formInputAccessoryViewController lockManualFallbackView];
  [self.formInputAccessoryMediator disableSuggestions];
}

#pragma mark - FallbackCoordinatorDelegate

- (void)fallbackCoordinatorDidDismissPopover:
    (FallbackCoordinator*)fallbackCoordinator {
  [self.formInputAccessoryMediator enableSuggestions];
  [self.formInputAccessoryViewController resetManualFallbackIcons];
}

#pragma mark - PasswordCoordinatorDelegate

- (void)openPasswordSettings {
  [self.delegate openPasswordSettings];
}

#pragma mark - CardCoordinatorDelegate

- (void)openCardSettings {
  [self.delegate openCreditCardSettings];
}

#pragma mark - AddressCoordinatorDelegate

- (void)openAddressSettings {
  [self.delegate openAddressSettings];
}

#pragma mark - AutofillSecurityAlertPresenter

- (void)presentSecurityWarningAlertWithText:(NSString*)body {
  NSString* alertTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_TITLE);
  NSString* defaultActionTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_NOT_SECURE_OK_BUTTON);

  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:alertTitle
                                          message:body
                                   preferredStyle:UIAlertControllerStyleAlert];
  UIAlertAction* defaultAction =
      [UIAlertAction actionWithTitle:defaultActionTitle
                               style:UIAlertActionStyleDefault
                             handler:^(UIAlertAction* action){
                             }];
  [alert addAction:defaultAction];
  [self.baseViewController presentViewController:alert
                                        animated:YES
                                      completion:nil];
}

@end
