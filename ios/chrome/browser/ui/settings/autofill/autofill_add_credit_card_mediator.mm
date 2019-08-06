// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/autofill/autofill_add_credit_card_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ios/chrome/browser/application_context.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type.h"
#import "ios/chrome/browser/ui/autofill/autofill_ui_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface AutofillAddCreditCardMediator ()

// Used for adding new CreditCard object.
@property(nonatomic, assign) autofill::PersonalDataManager* personalDataManager;

// This property is for an interface which sends a response about saving the
// credit card either the credit card is valid or it is invalid.
@property(nonatomic, weak) id<AddCreditCardMediatorDelegate>
    addCreditCardMediatorDelegate;

@end

@implementation AutofillAddCreditCardMediator

- (instancetype)initWithDelegate:(id<AddCreditCardMediatorDelegate>)
                                     addCreditCardMediatorDelegate
             personalDataManager:(autofill::PersonalDataManager*)dataManager {
  self = [super init];

  if (self) {
    DCHECK(dataManager);
    _personalDataManager = dataManager;
    _addCreditCardMediatorDelegate = addCreditCardMediatorDelegate;
  }

  return self;
}

#pragma mark - AddCreditCardViewControllerDelegate

- (void)addCreditCardViewController:(UIViewController*)viewController
        addCreditCardWithHolderName:(NSString*)cardHolderName
                         cardNumber:(NSString*)cardNumber
                    expirationMonth:(NSString*)expirationMonth
                     expirationYear:(NSString*)expirationYear {
  autofill::CreditCard creditCard = autofill::CreditCard();

  const std::string& appLocal = GetApplicationContext()->GetApplicationLocale();
  creditCard.SetInfo(autofill::AutofillType(AutofillTypeFromAutofillUIType(
                         AutofillUITypeCreditCardHolderFullName)),
                     base::SysNSStringToUTF16(cardHolderName), appLocal);

  creditCard.SetInfo(autofill::AutofillType(AutofillTypeFromAutofillUIType(
                         AutofillUITypeCreditCardNumber)),
                     base::SysNSStringToUTF16(cardNumber), appLocal);

  creditCard.SetInfo(autofill::AutofillType(AutofillTypeFromAutofillUIType(
                         AutofillUITypeCreditCardExpMonth)),
                     base::SysNSStringToUTF16(expirationMonth), appLocal);

  creditCard.SetInfo(autofill::AutofillType(AutofillTypeFromAutofillUIType(
                         AutofillUITypeCreditCardExpYear)),
                     base::SysNSStringToUTF16(expirationYear), appLocal);

  if (!creditCard.HasValidCardNumber()) {
    [self.addCreditCardMediatorDelegate
        creditCardMediatorHasInvalidCardNumber:self];
    return;
  }

  if (!creditCard.HasValidExpirationDate()) {
    [self.addCreditCardMediatorDelegate
        creditCardMediatorHasInvalidExpirationDate:self];
    return;
  }

  self.personalDataManager->AddCreditCard(creditCard);
  [self.addCreditCardMediatorDelegate creditCardMediatorDidFinish:self];
}

- (void)addCreditCardViewControllerDidCancel:(UIViewController*)viewController {
  [self.addCreditCardMediatorDelegate creditCardMediatorDidFinish:self];
}

@end
