// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_EXPIRATION_FIXER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_EXPIRATION_FIXER_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVCreditCard;

// Interface for responding to expiration date fix requests. This can happen
// when the user attempts to save a card with an expired expiration. Depending
// on the user's decision, either call |acceptWithMonth:year:| or |cancel|.
CWV_EXPORT
@interface CWVCreditCardExpirationFixer : NSObject

// The credit card whose expiration date needs fixing.
@property(nonatomic, readonly) CWVCreditCard* card;

// Title text to display in the header.
@property(nonatomic, readonly, copy) NSString* titleText;

// Label for the save button.
@property(nonatomic, readonly, copy) NSString* saveButtonLabel;

// Obfuscated label describing the |card|.
@property(nonatomic, readonly, copy) NSString* cardLabel;

// Label for the cancel button.
@property(nonatomic, readonly, copy) NSString* cancelButtonLabel;

// Label for the input control.
@property(nonatomic, readonly, copy) NSString* inputLabel;

// Separator to be used between the month and year.
@property(nonatomic, readonly, copy) NSString* dateSeparator;

// Error message describing why the date is invalid.
@property(nonatomic, readonly, copy) NSString* invalidDateErrorMessage;

- (instancetype)init NS_UNAVAILABLE;

// Accepts |month| and |year| as the new expiration.
// The expected format for month and year is MM and YYYY, respectively.
// Returns BOOL indicating whether or not the |month| and |year| are valid.
// May be called multiple times until the user provides a valid date.
- (BOOL)acceptWithMonth:(NSString*)month year:(NSString*)year;

// Cancels the expiration fix request. The card will not be saved.
// Do not call |acceptWithMonth:year:| after this.
- (void)cancel;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDIT_CARD_EXPIRATION_FIXER_H_
