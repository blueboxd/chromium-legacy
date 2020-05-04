// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_controller_internal.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/logging/stub_log_manager.h"
#include "components/autofill/core/browser/payments/test_strike_database.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_prefs.h"
#import "components/autofill/ios/browser/fake_autofill_agent.h"
#import "components/autofill/ios/browser/fake_js_autofill_manager.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#import "components/autofill/ios/browser/js_suggestion_manager.h"
#include "components/autofill/ios/form_util/form_activity_params.h"
#import "components/autofill/ios/form_util/form_activity_tab_helper.h"
#import "components/autofill/ios/form_util/test_form_activity_tab_helper.h"
#include "components/password_manager/core/browser/password_manager.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync/driver/test_sync_service.h"
#import "ios/web/public/deprecated/crw_test_js_injection_receiver.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#include "ios/web/public/test/fakes/test_browser_state.h"
#import "ios/web/public/test/fakes/test_web_state.h"
#include "ios/web/public/test/web_task_environment.h"
#import "ios/web_view/internal/autofill/cwv_autofill_suggestion_internal.h"
#import "ios/web_view/internal/autofill/web_view_autofill_client_ios.h"
#import "ios/web_view/internal/passwords/cwv_password_controller_fake.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_client.h"
#import "ios/web_view/internal/passwords/web_view_password_manager_driver.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/public/cwv_autofill_controller_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::test::ios::kWaitForActionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace ios_web_view {

namespace {

const char kApplicationLocale[] = "en-US";
NSString* const kTestFormName = @"FormName";
uint32_t kTestUniqueFormID = 0;
NSString* const kTestFieldIdentifier = @"FieldIdentifier";
uint32_t kTestUniqueFieldID = 1;
NSString* const kTestFrameId = @"FrameID";
NSString* const kTestFieldValue = @"FieldValue";
NSString* const kTestDisplayDescription = @"DisplayDescription";

}  // namespace

class CWVAutofillControllerTest : public PlatformTest {
 protected:
  CWVAutofillControllerTest() {
    pref_service_.registry()->RegisterBooleanPref(
        password_manager::prefs::kCredentialsEnableService, true);
    pref_service_.registry()->RegisterBooleanPref(
        autofill::prefs::kAutofillProfileEnabled, true);

    web_state_.SetBrowserState(&browser_state_);
    CRWTestJSInjectionReceiver* injectionReceiver =
        [[CRWTestJSInjectionReceiver alloc] init];
    web_state_.SetJSInjectionReceiver(injectionReceiver);

    js_autofill_manager_ =
        [[FakeJSAutofillManager alloc] initWithReceiver:injectionReceiver];
    js_suggestion_manager_ = OCMClassMock([JsSuggestionManager class]);

    autofill_agent_ =
        [[FakeAutofillAgent alloc] initWithPrefService:&pref_service_
                                              webState:&web_state_];

    auto frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = frames_manager.get();
    web_state_.SetWebFramesManager(std::move(frames_manager));

    // TODO(crbug.com/1070468): Redo CWVPasswordController so it is easier to
    // fake in unit tests.
    auto password_manager_client =
        std::make_unique<WebViewPasswordManagerClient>(
            &web_state_, /*sync_service=*/nullptr, &pref_service_,
            /*identity_manager=*/nullptr, /*log_manager=*/nullptr,
            /*profile_store=*/nullptr, /*account_store=*/nullptr);
    auto password_manager = std::make_unique<password_manager::PasswordManager>(
        password_manager_client.get());
    auto password_manager_driver =
        std::make_unique<WebViewPasswordManagerDriver>();
    password_controller_ = [[CWVPasswordControllerFake alloc]
             initWithWebState:&web_state_
              passwordManager:std::move(password_manager)
        passwordManagerClient:std::move(password_manager_client)
        passwordManagerDriver:std::move(password_manager_driver)];

    auto autofill_client = std::make_unique<autofill::WebViewAutofillClientIOS>(
        kApplicationLocale, &pref_service_, &personal_data_manager_,
        /*autocomplete_history_manager=*/nullptr, &web_state_,
        /*identity_manager=*/nullptr, &strike_database_, &sync_service_,
        std::make_unique<autofill::StubLogManager>());
    autofill_controller_ = [[CWVAutofillController alloc]
           initWithWebState:&web_state_
             autofillClient:std::move(autofill_client)
              autofillAgent:autofill_agent_
          JSAutofillManager:js_autofill_manager_
        JSSuggestionManager:js_suggestion_manager_
         passwordController:password_controller_
          applicationLocale:kApplicationLocale];
    form_activity_tab_helper_ =
        std::make_unique<autofill::TestFormActivityTabHelper>(&web_state_);
  }

  void AddWebFrame(std::unique_ptr<web::WebFrame> frame) {
    web::WebFrame* frame_ptr = frame.get();
    web_frames_manager_->AddWebFrame(std::move(frame));
    web_state_.OnWebFrameDidBecomeAvailable(frame_ptr);
  }

  web::WebTaskEnvironment task_environment_;
  TestingPrefServiceSimple pref_service_;
  web::TestBrowserState browser_state_;
  web::TestWebState web_state_;
  autofill::TestPersonalDataManager personal_data_manager_;
  autofill::TestStrikeDatabase strike_database_;
  syncer::TestSyncService sync_service_;
  web::FakeWebFramesManager* web_frames_manager_;
  CWVAutofillController* autofill_controller_;
  FakeAutofillAgent* autofill_agent_;
  FakeJSAutofillManager* js_autofill_manager_;
  CWVPasswordControllerFake* password_controller_;
  std::unique_ptr<autofill::TestFormActivityTabHelper>
      form_activity_tab_helper_;
  id js_suggestion_manager_;
};

// Tests CWVAutofillController fetch suggestions for profiles.
TEST_F(CWVAutofillControllerTest, FetchProfileSuggestions) {
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:kTestFieldValue
                       displayDescription:kTestDisplayDescription
                                     icon:nil
                               identifier:0];
  [autofill_agent_ addSuggestion:suggestion
                     forFormName:kTestFormName
                 fieldIdentifier:kTestFieldIdentifier
                         frameID:kTestFrameId];

  __block BOOL fetch_completion_was_called = NO;
  id fetch_completion = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    ASSERT_EQ(1U, suggestions.count);
    CWVAutofillSuggestion* suggestion = suggestions.firstObject;
    EXPECT_NSEQ(kTestFieldValue, suggestion.value);
    EXPECT_NSEQ(kTestDisplayDescription, suggestion.displayDescription);
    EXPECT_NSEQ(kTestFormName, suggestion.formName);
    fetch_completion_was_called = YES;
  };
  [autofill_controller_ fetchSuggestionsForFormWithName:kTestFormName
                                        fieldIdentifier:kTestFieldIdentifier
                                              fieldType:@""
                                                frameID:kTestFrameId
                                      completionHandler:fetch_completion];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fetch_completion_was_called;
  }));
}

// Tests CWVAutofillController fetch suggestions for passwords.
TEST_F(CWVAutofillControllerTest, FetchPasswordSuggestions) {
  FormSuggestion* suggestion =
      [FormSuggestion suggestionWithValue:kTestFieldValue
                       displayDescription:nil
                                     icon:nil
                               identifier:0];
  [autofill_agent_ addSuggestion:suggestion
                     forFormName:kTestFormName
                 fieldIdentifier:kTestFieldIdentifier
                         frameID:kTestFrameId];

  CWVAutofillSuggestion* passwordSuggestion =
      [[CWVAutofillSuggestion alloc] initWithFormSuggestion:suggestion
                                                   formName:kTestFormName
                                            fieldIdentifier:kTestFieldIdentifier
                                                    frameID:kTestFrameId
                                       isPasswordSuggestion:YES];
  [password_controller_ addPasswordSuggestion:passwordSuggestion
                                     formName:kTestFormName
                              fieldIdentifier:kTestFieldIdentifier
                                      frameID:kTestFrameId];

  __block BOOL fetch_completion_was_called = NO;
  id fetch_completion = ^(NSArray<CWVAutofillSuggestion*>* suggestions) {
    // Even though there are two available suggestions, having password
    // suggestions means no profile suggestions should be returned.
    ASSERT_EQ(1U, suggestions.count);
    CWVAutofillSuggestion* suggestion = suggestions.firstObject;
    EXPECT_TRUE([suggestion isPasswordSuggestion]);
    EXPECT_NSEQ(kTestFieldValue, suggestion.value);
    EXPECT_NSEQ(kTestFormName, suggestion.formName);
    fetch_completion_was_called = YES;
  };
  [autofill_controller_ fetchSuggestionsForFormWithName:kTestFormName
                                        fieldIdentifier:kTestFieldIdentifier
                                              fieldType:@""
                                                frameID:kTestFrameId
                                      completionHandler:fetch_completion];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return fetch_completion_was_called;
  }));
}

// Tests CWVAutofillController accepts suggestion.
TEST_F(CWVAutofillControllerTest, AcceptSuggestion) {
  FormSuggestion* form_suggestion =
      [FormSuggestion suggestionWithValue:kTestFieldValue
                       displayDescription:nil
                                     icon:nil
                               identifier:0];
  CWVAutofillSuggestion* suggestion =
      [[CWVAutofillSuggestion alloc] initWithFormSuggestion:form_suggestion
                                                   formName:kTestFormName
                                            fieldIdentifier:kTestFieldIdentifier
                                                    frameID:kTestFrameId
                                       isPasswordSuggestion:NO];
  __block BOOL accept_completion_was_called = NO;
  [autofill_controller_ acceptSuggestion:suggestion
                       completionHandler:^{
                         accept_completion_was_called = YES;
                       }];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return accept_completion_was_called;
  }));
  EXPECT_NSEQ(
      form_suggestion,
      [autofill_agent_ selectedSuggestionForFormName:kTestFormName
                                     fieldIdentifier:kTestFieldIdentifier
                                             frameID:kTestFrameId]);
}

// Tests CWVAutofillController clears form.
TEST_F(CWVAutofillControllerTest, ClearForm) {
  auto frame = std::make_unique<web::FakeWebFrame>(
      base::SysNSStringToUTF8(kTestFrameId), true, GURL::EmptyGURL());
  AddWebFrame(std::move(frame));
  __block BOOL clear_form_completion_was_called = NO;
  [autofill_controller_ clearFormWithName:kTestFormName
                          fieldIdentifier:kTestFieldIdentifier
                                  frameID:kTestFrameId
                        completionHandler:^{
                          clear_form_completion_was_called = YES;
                        }];

  EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForActionTimeout, ^bool {
    base::RunLoop().RunUntilIdle();
    return clear_form_completion_was_called;
  }));
  EXPECT_NSEQ(kTestFormName, js_autofill_manager_.lastClearedFormName);
  EXPECT_NSEQ(kTestFieldIdentifier,
              js_autofill_manager_.lastClearedFieldIdentifier);
  EXPECT_NSEQ(kTestFrameId, js_autofill_manager_.lastClearedFrameIdentifier);
}

// Tests CWVAutofillController focus previous field.
TEST_F(CWVAutofillControllerTest, FocusPrevious) {
  [[js_suggestion_manager_ expect] selectPreviousElementInFrameWithID:nil];
  [autofill_controller_ focusPreviousField];
  [js_suggestion_manager_ verify];
}

// Tests CWVAutofillController focus next field.
TEST_F(CWVAutofillControllerTest, FocusNext) {
  [[js_suggestion_manager_ expect] selectNextElementInFrameWithID:nil];
  [autofill_controller_ focusNextField];
  [js_suggestion_manager_ verify];
}

// Tests CWVAutofillController checks previous and next focusable state.
TEST_F(CWVAutofillControllerTest, CheckFocus) {
  id completionHandler = ^(BOOL previous, BOOL next) {
  };
  [[js_suggestion_manager_ expect]
      fetchPreviousAndNextElementsPresenceInFrameWithID:nil
                                      completionHandler:completionHandler];
  [autofill_controller_
      checkIfPreviousAndNextFieldsAreAvailableForFocusWithCompletionHandler:
          completionHandler];
  [js_suggestion_manager_ verify];
}

// Tests CWVAutofillController delegate focus callback is invoked.
TEST_F(CWVAutofillControllerTest, FocusCallback) {
    id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
    autofill_controller_.delegate = delegate;

    [[delegate expect] autofillController:autofill_controller_
            didFocusOnFieldWithIdentifier:kTestFieldIdentifier
                                fieldType:@""
                                 formName:kTestFormName
                                  frameID:kTestFrameId
                                    value:kTestFieldValue
                            userInitiated:YES];

    autofill::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.unique_form_id = kTestUniqueFormID;
    params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
    params.unique_field_id = kTestUniqueFieldID;
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.frame_id = base::SysNSStringToUTF8(kTestFrameId);
    params.has_user_gesture = true;
    params.type = "focus";
    web::FakeWebFrame frame(base::SysNSStringToUTF8(kTestFrameId), true,
                            GURL::EmptyGURL());
    form_activity_tab_helper_->FormActivityRegistered(&frame, params);
    [delegate verify];
}

// Tests CWVAutofillController delegate input callback is invoked.
TEST_F(CWVAutofillControllerTest, InputCallback) {
    id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
    autofill_controller_.delegate = delegate;

    [[delegate expect] autofillController:autofill_controller_
            didInputInFieldWithIdentifier:kTestFieldIdentifier
                                fieldType:@""
                                 formName:kTestFormName
                                  frameID:kTestFrameId
                                    value:kTestFieldValue
                            userInitiated:YES];

    autofill::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.frame_id = base::SysNSStringToUTF8(kTestFrameId);
    params.type = "input";
    params.has_user_gesture = true;
    web::FakeWebFrame frame(base::SysNSStringToUTF8(kTestFrameId), true,
                            GURL::EmptyGURL());
    form_activity_tab_helper_->FormActivityRegistered(&frame, params);
    [delegate verify];
}

// Tests CWVAutofillController delegate blur callback is invoked.
TEST_F(CWVAutofillControllerTest, BlurCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

    [[delegate expect] autofillController:autofill_controller_
             didBlurOnFieldWithIdentifier:kTestFieldIdentifier
                                fieldType:@""
                                 formName:kTestFormName
                                  frameID:kTestFrameId
                                    value:kTestFieldValue
                            userInitiated:YES];

    autofill::FormActivityParams params;
    params.form_name = base::SysNSStringToUTF8(kTestFormName);
    params.field_identifier = base::SysNSStringToUTF8(kTestFieldIdentifier);
    params.value = base::SysNSStringToUTF8(kTestFieldValue);
    params.frame_id = base::SysNSStringToUTF8(kTestFrameId);
    params.type = "blur";
    params.has_user_gesture = true;
    web::FakeWebFrame frame(base::SysNSStringToUTF8(kTestFrameId), true,
                            GURL::EmptyGURL());
    form_activity_tab_helper_->FormActivityRegistered(&frame, params);

    [delegate verify];
}

// Tests CWVAutofillController delegate submit callback is invoked.
TEST_F(CWVAutofillControllerTest, SubmitCallback) {
  id delegate = OCMProtocolMock(@protocol(CWVAutofillControllerDelegate));
  autofill_controller_.delegate = delegate;

    [[delegate expect] autofillController:autofill_controller_
                    didSubmitFormWithName:kTestFormName
                                  frameID:kTestFrameId
                            userInitiated:YES];
    web::FakeWebFrame frame(base::SysNSStringToUTF8(kTestFrameId), true,
                            GURL::EmptyGURL());
    form_activity_tab_helper_->DocumentSubmitted(
        /*sender_frame*/ &frame, base::SysNSStringToUTF8(kTestFormName),
        /*form_data=*/"",
        /*user_initiated=*/true,
        /*is_main_frame=*/true);

    [[delegate expect] autofillController:autofill_controller_
                    didSubmitFormWithName:kTestFormName
                                  frameID:kTestFrameId
                            userInitiated:NO];

    form_activity_tab_helper_->DocumentSubmitted(
        /*sender_frame*/ &frame, base::SysNSStringToUTF8(kTestFormName),
        /*form_data=*/"",
        /*user_initiated=*/false,
        /*is_main_frame=*/true);

    [delegate verify];
}

}  // namespace ios_web_view
