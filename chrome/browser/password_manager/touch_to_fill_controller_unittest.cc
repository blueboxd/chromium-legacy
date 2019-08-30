// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/touch_to_fill_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autofill/mock_manual_filling_controller.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using password_manager::CredentialPair;

struct MockPasswordManagerDriver : password_manager::StubPasswordManagerDriver {
  MOCK_METHOD2(FillSuggestion,
               void(const base::string16&, const base::string16&));
};

class TouchToFillControllerTest : public testing::Test {
 protected:
  MockManualFillingController& manual_filling_controller() {
    return mock_manual_filling_controller_;
  }

  MockPasswordManagerDriver& driver() { return driver_; }

  TouchToFillController& touch_to_fill_controller() {
    return touch_to_fill_controller_;
  }

 private:
  testing::StrictMock<MockManualFillingController>
      mock_manual_filling_controller_;
  MockPasswordManagerDriver driver_;
  TouchToFillController touch_to_fill_controller_{
      mock_manual_filling_controller_.AsWeakPtr(),
      util::PassKey<TouchToFillControllerTest>()};
};

TEST_F(TouchToFillControllerTest, Show_Empty) {
  EXPECT_CALL(manual_filling_controller(),
              RefreshSuggestions(autofill::AccessorySheetData::Builder(
                                     autofill::AccessoryTabType::TOUCH_TO_FILL,
                                     base::ASCIIToUTF16("Touch to Fill"))
                                     .Build()));
  touch_to_fill_controller().Show({}, driver().AsWeakPtr());
}

TEST_F(TouchToFillControllerTest, Show_And_Fill) {
  CredentialPair alice(
      base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
      GURL("https://example.com"), /*is_public_suffix_match=*/false);

  EXPECT_CALL(
      manual_filling_controller(),
      RefreshSuggestions(
          autofill::AccessorySheetData::Builder(
              autofill::AccessoryTabType::TOUCH_TO_FILL,
              base::ASCIIToUTF16("Touch to Fill"))
              .AddUserInfo()
              .AppendField(base::ASCIIToUTF16("alice"),
                           base::ASCIIToUTF16("alice"), "0",
                           /*is_obfuscated=*/false, /*is_selectable=*/true)
              .AppendField(base::ASCIIToUTF16("p4ssw0rd"),
                           base::ASCIIToUTF16("p4ssw0rd"), "0",
                           /*is_obfuscated=*/true, /*is_selectable=*/false)
              .Build()));
  touch_to_fill_controller().Show(std::vector<CredentialPair>{alice},
                                  driver().AsWeakPtr());

  // Test that OnFillingTriggered() with the right id results in the appropriate
  // call to FillSuggestion() and UpdateSourceAvailability().
  EXPECT_CALL(driver(), FillSuggestion(base::ASCIIToUTF16("alice"),
                                       base::ASCIIToUTF16("p4ssw0rd")));
  EXPECT_CALL(manual_filling_controller(),
              UpdateSourceAvailability(
                  ManualFillingController::FillingSource::TOUCH_TO_FILL,
                  /*has_suggestions=*/false));

  // Test that we correctly log the absence of an Android credential.
  base::HistogramTester tester;
  touch_to_fill_controller().OnFillingTriggered(autofill::UserInfo::Field(
      base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("alice"), "0",
      /*is_obfuscated=*/false, /*is_selectable=*/true));
  tester.ExpectUniqueSample("PasswordManager.FilledCredentialWasFromAndroidApp",
                            false, 1);
}

TEST_F(TouchToFillControllerTest, Show_PSL_Credential) {
  // Test that showing a PSL credentials results in the origin being passed to
  // the manual filling controller.
  CredentialPair alice(
      base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
      GURL("https://sub.example.com/"), /*is_public_suffix_match=*/true);

  EXPECT_CALL(
      manual_filling_controller(),
      RefreshSuggestions(
          autofill::AccessorySheetData::Builder(
              autofill::AccessoryTabType::TOUCH_TO_FILL,
              base::ASCIIToUTF16("Touch to Fill"))
              .AddUserInfo("https://sub.example.com/")
              .AppendField(base::ASCIIToUTF16("alice"),
                           base::ASCIIToUTF16("alice"), "0",
                           /*is_obfuscated=*/false, /*is_selectable=*/true)
              .AppendField(base::ASCIIToUTF16("p4ssw0rd"),
                           base::ASCIIToUTF16("p4ssw0rd"), "0",
                           /*is_obfuscated=*/true, /*is_selectable=*/false)
              .Build()));
  touch_to_fill_controller().Show(std::vector<CredentialPair>{alice},
                                  driver().AsWeakPtr());
}

TEST_F(TouchToFillControllerTest, Show_And_Fill_Android_Credential) {
  // Test multiple credentials with one of them being an Android credential.
  CredentialPair alice(
      base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
      GURL("https://example.com"), /*is_public_suffix_match=*/false);

  CredentialPair bob(base::ASCIIToUTF16("bob"), base::ASCIIToUTF16("s3cr3t"),
                     GURL("android://hash@com.example.my"),
                     /*is_public_suffix_match=*/false);

  EXPECT_CALL(
      manual_filling_controller(),
      RefreshSuggestions(
          autofill::AccessorySheetData::Builder(
              autofill::AccessoryTabType::TOUCH_TO_FILL,
              base::ASCIIToUTF16("Touch to Fill"))
              .AddUserInfo()
              .AppendField(base::ASCIIToUTF16("alice"),
                           base::ASCIIToUTF16("alice"), "0",
                           /*is_obfuscated=*/false, /*is_selectable=*/true)
              .AppendField(base::ASCIIToUTF16("p4ssw0rd"),
                           base::ASCIIToUTF16("p4ssw0rd"), "0",
                           /*is_obfuscated=*/true, /*is_selectable=*/false)
              .AddUserInfo()
              .AppendField(base::ASCIIToUTF16("bob"), base::ASCIIToUTF16("bob"),
                           "1", /*is_obfuscated=*/false, /*is_selectable=*/true)
              .AppendField(base::ASCIIToUTF16("s3cr3t"),
                           base::ASCIIToUTF16("s3cr3t"), "1",
                           /*is_obfuscated=*/true, /*is_selectable=*/false)
              .Build()));
  touch_to_fill_controller().Show(std::vector<CredentialPair>{alice, bob},
                                  driver().AsWeakPtr());

  EXPECT_CALL(driver(), FillSuggestion(base::ASCIIToUTF16("bob"),
                                       base::ASCIIToUTF16("s3cr3t")));
  EXPECT_CALL(manual_filling_controller(),
              UpdateSourceAvailability(
                  ManualFillingController::FillingSource::TOUCH_TO_FILL,
                  /*has_suggestions=*/false));

  // Test that we correctly log the presence of an Android credential.
  base::HistogramTester tester;
  touch_to_fill_controller().OnFillingTriggered(autofill::UserInfo::Field(
      base::ASCIIToUTF16("bob"), base::ASCIIToUTF16("bob"), "1",
      /*is_obfuscated=*/false, /*is_selectable=*/true));
  tester.ExpectUniqueSample("PasswordManager.FilledCredentialWasFromAndroidApp",
                            true, 1);
}
