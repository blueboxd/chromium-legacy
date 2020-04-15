// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CONTROLLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CONTROLLER_INTERNAL_H_

#include <memory>
#include <string>

#import "ios/web_view/public/cwv_autofill_controller.h"

NS_ASSUME_NONNULL_BEGIN

namespace autofill {
class WebViewAutofillClientIOS;
}  // namespace autofill

namespace web {
class WebState;
}  // namespace web

@class AutofillAgent;
@class CWVPasswordController;
@class JsAutofillManager;
@class JsSuggestionManager;

@interface CWVAutofillController ()

- (instancetype)initWithWebState:(web::WebState*)webState
                  autofillClient:
                      (std::unique_ptr<autofill::WebViewAutofillClientIOS>)
                          autofillClient
                   autofillAgent:(AutofillAgent*)autofillAgent
               JSAutofillManager:(JsAutofillManager*)JSAutofillManager
             JSSuggestionManager:(JsSuggestionManager*)JSSuggestionManager
              passwordController:(CWVPasswordController*)passwordController
               applicationLocale:(const std::string&)applicationLocale
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_AUTOFILL_CWV_AUTOFILL_CONTROLLER_INTERNAL_H_
