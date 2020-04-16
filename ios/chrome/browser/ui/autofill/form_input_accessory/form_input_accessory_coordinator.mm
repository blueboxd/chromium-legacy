// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_coordinator.h"

#include <vector>

#include "base/ios/ios_util.h"
#include "base/mac/foundation_util.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#include "ios/chrome/browser/passwords/ios_chrome_password_store_factory.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_mediator.h"
#import "ios/chrome/browser/ui/autofill/form_input_accessory/form_input_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/address_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/fallback_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_accessory_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_password_coordinator.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface FormInputAccessoryCoordinator () <
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
@property(nonatomic, strong) ManualFillInjectionHandler* injectionHandler;

@end

@implementation FormInputAccessoryCoordinator

- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                          injectionHandler:
                              (ManualFillInjectionHandler*)injectionHandler {
  self = [super initWithBaseViewController:viewController browser:browser];
  if (self) {
    _injectionHandler = injectionHandler;
  }
  return self;
}

- (void)start {
  _formInputAccessoryViewController = [[FormInputAccessoryViewController alloc]
      initWithManualFillAccessoryViewControllerDelegate:self];

  auto passwordStore = IOSChromePasswordStoreFactory::GetForBrowserState(
      self.browser->GetBrowserState(), ServiceAccessType::EXPLICIT_ACCESS);

  // There is no personal data manager in OTR (incognito). Get the original
  // one for manual fallback.
  autofill::PersonalDataManager* personalDataManager =
      autofill::PersonalDataManagerFactory::GetForBrowserState(
          self.browser->GetBrowserState()->GetOriginalChromeBrowserState());

  _formInputAccessoryMediator = [[FormInputAccessoryMediator alloc]
         initWithConsumer:self.formInputAccessoryViewController
                 delegate:self
             webStateList:self.browser->GetWebStateList()
      personalDataManager:personalDataManager
            passwordStore:passwordStore];
}

- (void)stop {
  [self stopChildren];

  [self.formInputAccessoryViewController restoreOriginalKeyboardView];
  self.formInputAccessoryViewController = nil;

  [self.formInputAccessoryMediator disconnect];
  self.formInputAccessoryMediator = nil;
}

- (void)reset {
  [self stopChildren];
  [self.formInputAccessoryMediator enableSuggestions];
  [self.formInputAccessoryViewController reset];
}

#pragma mark - Presenting Children

- (void)stopChildren {
  for (ChromeCoordinator* coordinator in self.childCoordinators) {
    [coordinator stop];
  }
  [self.childCoordinators removeAllObjects];
}

- (void)startPasswordsFromButton:(UIButton*)button {
  WebStateList* webStateList = self.browser->GetWebStateList();
  DCHECK(webStateList->GetActiveWebState());
  const GURL& URL = webStateList->GetActiveWebState()->GetLastCommittedURL();
  ManualFillPasswordCoordinator* passwordCoordinator =
      [[ManualFillPasswordCoordinator alloc]
          initWithBaseViewController:self.baseViewController
                             browser:self.browser
                                 URL:URL
                    injectionHandler:self.injectionHandler];
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
                         browser:self.browser
                injectionHandler:self.injectionHandler];
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
                         browser:self.browser
                injectionHandler:self.injectionHandler];
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
    [self reset];
  }
}

- (void)mediatorDidDetectMovingToBackground:
    (FormInputAccessoryMediator*)mediator {
  [self reset];
}

#pragma mark - ManualFillAccessoryViewControllerDelegate

- (void)keyboardButtonPressed {
  [self reset];
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
  [self reset];
}

#pragma mark - PasswordCoordinatorDelegate

- (void)openPasswordSettings {
  [self reset];
  [self.navigator openPasswordSettings];
}

- (void)openAllPasswordsPicker {
  [self reset];
  [self.navigator openAllPasswordsPicker];
}

#pragma mark - CardCoordinatorDelegate

- (void)openCardSettings {
  [self reset];
  [self.navigator openCreditCardSettings];
}

#pragma mark - AddressCoordinatorDelegate

- (void)openAddressSettings {
  [self reset];
  [self.navigator openAddressSettings];
}

@end
