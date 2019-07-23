// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/touch_to_fill_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/mock_autofill_popup_controller.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::MockAutofillPopupController;

class TouchToFillControllerTest : public testing::Test {
 protected:
  MockManualFillingController& manual_filling_controller() {
    return mock_manual_filling_controller_;
  }

  MockAutofillPopupController& popup_controller() {
    return mock_popup_controller_;
  }

  TouchToFillController* touch_to_fill_controller() {
    return touch_to_fill_controller_.get();
  }

 private:
  testing::StrictMock<MockManualFillingController>
      mock_manual_filling_controller_;
  MockAutofillPopupController mock_popup_controller_;
  std::unique_ptr<TouchToFillController> touch_to_fill_controller_ =
      TouchToFillController::CreateForTesting(
          mock_manual_filling_controller_.AsWeakPtr());
};

TEST_F(TouchToFillControllerTest, AllowedForWebContents) {
  for (bool is_touch_to_fill_enabled : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "is_touch_to_fill_enabled: " << std::boolalpha
                 << is_touch_to_fill_enabled);
    base::test::ScopedFeatureList scoped_feature_list;
    scoped_feature_list.InitWithFeatureState(
        autofill::features::kTouchToFillAndroid, is_touch_to_fill_enabled);
    EXPECT_EQ(is_touch_to_fill_enabled,
              TouchToFillController::AllowedForWebContents(nullptr));
  }
}

TEST_F(TouchToFillControllerTest, Show) {
  // Test the appropriate translation of autofill suggestions into
  // AccessorySheetData. Use masked passwords to mirror production behavior.
  // Test both empty and non-empty realms.
  const base::string16 alice_user = base::ASCIIToUTF16("Alice");
  const base::string16 alice_pass = base::ASCIIToUTF16("*****");
  const base::string16 alice_realm;

  const base::string16 bob_user = base::ASCIIToUTF16("Bob");
  const base::string16 bob_pass = base::ASCIIToUTF16("***");
  const base::string16 bob_realm = base::ASCIIToUTF16("https://example.com");

  autofill::Suggestion alice(alice_user);
  alice.additional_label = alice_pass;
  alice.frontend_id = autofill::POPUP_ITEM_ID_USERNAME_ENTRY;

  autofill::Suggestion bob(bob_user);
  bob.additional_label = bob_pass;
  bob.label = bob_realm;
  bob.frontend_id = autofill::POPUP_ITEM_ID_PASSWORD_ENTRY;

  // Add an "All Saved Passwords" entry, which should be ignored.
  autofill::Suggestion all_passwords;
  all_passwords.frontend_id = autofill::POPUP_ITEM_ID_ALL_SAVED_PASSWORDS_ENTRY;
  popup_controller().set_suggestions({alice, bob, all_passwords});

  EXPECT_CALL(manual_filling_controller(),
              RefreshSuggestions(
                  autofill::AccessorySheetData::Builder(
                      autofill::AccessoryTabType::TOUCH_TO_FILL,
                      base::ASCIIToUTF16("Touch to Fill"))
                      .AddUserInfo()
                      .AppendField(alice_user, alice_user, "0", false, true)
                      .AppendField(alice_pass, alice_pass, "0", true, false)
                      .AppendField(alice_realm, alice_realm, "0", false, false)
                      .AddUserInfo()
                      .AppendField(bob_user, bob_user, "1", false, true)
                      .AppendField(bob_pass, bob_pass, "1", true, false)
                      .AppendField(bob_realm, bob_realm, "1", false, false)
                      .Build()));
  touch_to_fill_controller()->Show(popup_controller().GetSuggestions(),
                                   popup_controller().AsWeakPtr());

  EXPECT_CALL(popup_controller(), AcceptSuggestion(1));
  touch_to_fill_controller()->OnFillingTriggered(
      autofill::UserInfo::Field(bob_user, bob_user, "1", false, true));
}
