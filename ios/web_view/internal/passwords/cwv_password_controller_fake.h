// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_FAKE_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_FAKE_H_

#import "ios/web_view/internal/passwords/cwv_password_controller.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVAutofillSuggestion;

// Fake password controller used in unit tests.
@interface CWVPasswordControllerFake : CWVPasswordController

// Adds a password suggestion to be returned when suggestions are fetched.
- (void)addPasswordSuggestion:(CWVAutofillSuggestion*)suggestion
                     formName:(NSString*)formName
              fieldIdentifier:(NSString*)fieldIdentifier
                      frameID:(NSString*)frameID;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_FAKE_H_
