// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_LABEL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_LABEL_H_

#import <UIKit/UIKit.h>

// a11y identifier used to locate the autofill suggestion in automation
extern NSString* const kFormSuggestionLabelAccessibilityIdentifier;

@class FormSuggestion;
@protocol FormSuggestionClient;

// Class for Autofill suggestion in the customized keyboard.
@interface FormSuggestionLabel : UIView

// Designated initializer. Initializes with |client| for |suggestion|.
- (instancetype)initWithSuggestion:(FormSuggestion*)suggestion
                             index:(NSUInteger)index
                    numSuggestions:(NSUInteger)numSuggestions
                            client:(id<FormSuggestionClient>)client
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
@end

#endif  // IOS_CHROME_BROWSER_AUTOFILL_FORM_SUGGESTION_LABEL_H_
