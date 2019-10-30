// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/form_input_accessory_view_handler.h"

#include "base/mac/foundation_util.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#import "ios/web/public/test/web_test_with_web_state.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

class FormInputAccessoryViewHandlerTest : public web::WebTestWithWebState {};

// Tests that trying to programmatically dismiss the keyboard when it isn't
// visible doesn't crash the browser.
TEST_F(FormInputAccessoryViewHandlerTest, FormInputAccessoryViewHandler) {
  FormInputAccessoryViewHandler* accessoryViewDelegate =
      [[FormInputAccessoryViewHandler alloc] init];
  ASSERT_TRUE(accessoryViewDelegate);
  [accessoryViewDelegate closeKeyboardWithoutButtonPress];
  CRWJSInjectionReceiver* injectionReceiver =
      web_state()->GetJSInjectionReceiver();
  accessoryViewDelegate.JSSuggestionManager =
      base::mac::ObjCCastStrict<JsSuggestionManager>(
          [injectionReceiver instanceOfClass:[JsSuggestionManager class]]);
  [accessoryViewDelegate closeKeyboardWithoutButtonPress];
}
