// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_DELEGATE_H_

@class UIViewController;

// Delegate manages adding a new credit card.
@protocol AddCreditCardViewControllerDelegate

// Receives a credit card data. Implement this method to save a new credit card.
- (void)addCreditCardViewController:(UIViewController*)viewController
        addCreditCardWithHolderName:(NSString*)cardHolderName
                         cardNumber:(NSString*)cardNumber
                    expirationMonth:(NSString*)expirationMonth
                     expirationYear:(NSString*)expirationYear;

// Notifies the class which conform this delegate for cancel button tap in
// received view controller.
- (void)addCreditCardViewControllerDidCancel:(UIViewController*)viewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_AUTOFILL_AUTOFILL_ADD_CREDIT_CARD_VIEW_CONTROLLER_DELEGATE_H_
