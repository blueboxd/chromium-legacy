// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_H_

#import <Foundation/Foundation.h>

#include <memory>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, CWVPasswordUserDecision);
@class CWVAutofillSuggestion;
@class CWVPasswordController;
@class CWVPassword;

namespace ios_web_view {
class WebViewPasswordManagerClient;
class WebViewPasswordManagerDriver;
}  // namespace ios_web_view

namespace password_manager {
class PasswordManager;
}  // namespace password_manager

namespace web {
class WebState;
}  // namespace web

namespace autofill {
class FormRendererId;
class FieldRendererId;
}  // namespace autofill

// Internal protocol to receive callbacks related to password autofilling.
@protocol CWVPasswordControllerDelegate

// Called when user needs to decide on whether or not to save the |password|.
// This can happen when user is successfully logging into a web site with a new
// username.
// Pass user decision to |decisionHandler|. This block should be called only
// once if user made the decision, or not get called if user ignores the prompt.
- (void)passwordController:(CWVPasswordController*)passwordController
    decideSavePolicyForPassword:(CWVPassword*)password
                decisionHandler:
                    (void (^)(CWVPasswordUserDecision decision))decisionHandler;

// Called when user needs to decide on whether or not to update the |password|.
// This can happen when user is successfully logging into a web site with a new
// password and an existing username.
// Pass user decision to |decisionHandler|. This block should be called only
// once if user made the decision, or not get called if user ignores the prompt.
- (void)passwordController:(CWVPasswordController*)passwordController
    decideUpdatePolicyForPassword:(CWVPassword*)password
                  decisionHandler:(void (^)(CWVPasswordUserDecision decision))
                                      decisionHandler;

@end

// Implements features that allow saving entered passwords as well as
// autofilling password forms.
@interface CWVPasswordController : NSObject

// |delegate| is used to receive password autofill suggestion callbacks.
@property(nonatomic, weak, nullable) id<CWVPasswordControllerDelegate> delegate;

// Creates a new password controller with the given |webState|.
- (instancetype)initWithWebState:(web::WebState*)webState
                 passwordManager:
                     (std::unique_ptr<password_manager::PasswordManager>)
                         passwordManager
           passwordManagerClient:
               (std::unique_ptr<ios_web_view::WebViewPasswordManagerClient>)
                   passwordManagerClient
           passwordManagerDriver:
               (std::unique_ptr<ios_web_view::WebViewPasswordManagerDriver>)
                   passwordManagerDriver NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// See same method in |CWVAutofillController|. This one only fetches password
// autofill suggestions.
- (void)fetchSuggestionsForFormWithName:(NSString*)formName
                           uniqueFormID:(autofill::FormRendererId)uniqueFormID
                        fieldIdentifier:(NSString*)fieldIdentifier
                          uniqueFieldID:(autofill::FieldRendererId)uniqueFieldID
                              fieldType:(NSString*)fieldType
                                frameID:(NSString*)frameID
                      completionHandler:
                          (void (^)(NSArray<CWVAutofillSuggestion*>*))
                              completionHandler;

// See same method in |CWVAutofillController|. This one autofills password form
// on the page.
- (void)fillSuggestion:(CWVAutofillSuggestion*)suggestion
     completionHandler:(void (^)(void))completionHandler;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_CWV_PASSWORD_CONTROLLER_H_
