// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_NAME_FIXER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_NAME_FIXER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Interface for responding to name fix requests when saving a credit card. This
// can happen if a card was entered without a name. Depending on the user's
// decision, either call |acceptWithName:| or |cancel|, not both.
CWV_EXPORT
@interface CWVCreditCardNameFixer : NSObject

// The suggested card holder name, for example from the user's Google Account.
// Can be empty if there's no name to suggest.
@property(nonatomic, readonly, copy) NSString* inferredCardHolderName;

// Label for cancel button.
@property(nonatomic, readonly, copy) NSString* cancelButtonLabel;

// Extra explanatory text for |inferredCardHolderName|.
@property(nonatomic, readonly, copy) NSString* inferredNameTooltipText;

// Label for the input control.
@property(nonatomic, readonly, copy) NSString* inputLabel;

// Placeholder text for use in the input control.
@property(nonatomic, readonly, copy) NSString* inputPlaceholderText;

// Label for the save button.
@property(nonatomic, readonly, copy) NSString* saveButtonLabel;

// Title for the navigation bar.
@property(nonatomic, readonly, copy) NSString* titleText;

- (instancetype)init NS_UNAVAILABLE;

// Accepts |name| as the name to associate with the card.
- (void)acceptWithName:(NSString*)name;

// Cancels the name fix. The card will not be saved.
- (void)cancel;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_NAME_FIXER_H_
