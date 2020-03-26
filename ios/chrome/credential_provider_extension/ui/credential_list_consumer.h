// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_CONSUMER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_CONSUMER_H_

@class UIButton;

@protocol CredentialListConsumerDelegate <NSObject>

// Called when the user taps the cancel button in the navigation bar.
- (void)navigationCancelButtonWasPressed:(UIButton*)button;

// Called when the user is filtering results through search.
- (void)updateResultsWithFilter:(NSString*)filter;

@end

@protocol CredentialListConsumer <NSObject>

// The delegate for the actions in the consumer.
@property(nonatomic, weak) id<CredentialListConsumerDelegate> delegate;

@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_UI_CREDENTIAL_LIST_CONSUMER_H_
