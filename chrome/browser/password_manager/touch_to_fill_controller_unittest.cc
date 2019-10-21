// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/touch_to_fill_controller.h"

#include <memory>
#include <tuple>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/origin_credential_store.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using password_manager::CredentialPair;
using ::testing::_;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::ReturnRefOfCopy;
using ::testing::WithArg;
using IsOriginSecure = TouchToFillView::IsOriginSecure;

using IsPublicSuffixMatch = CredentialPair::IsPublicSuffixMatch;

constexpr char kExampleCom[] = "https://example.com/";

struct MockPasswordManagerDriver : password_manager::StubPasswordManagerDriver {
  MOCK_METHOD2(FillSuggestion,
               void(const base::string16&, const base::string16&));
  MOCK_METHOD0(TouchToFillDismissed, void());
  MOCK_CONST_METHOD0(GetLastCommittedURL, const GURL&());
};

struct MockTouchToFillView : TouchToFillView {
  MOCK_METHOD3(Show,
               void(base::StringPiece16,
                    IsOriginSecure,
                    base::span<const CredentialPair>));
  MOCK_METHOD1(OnCredentialSelected, void(const CredentialPair&));
  MOCK_METHOD0(OnDismiss, void());
};

}  // namespace

class TouchToFillControllerTest : public testing::Test {
 protected:
  TouchToFillControllerTest() {
    auto mock_view = std::make_unique<MockTouchToFillView>();
    mock_view_ = mock_view.get();
    touch_to_fill_controller_.set_view(std::move(mock_view));

    ON_CALL(driver_, GetLastCommittedURL())
        .WillByDefault(ReturnRefOfCopy(GURL(kExampleCom)));
  }

  MockPasswordManagerDriver& driver() { return driver_; }

  MockTouchToFillView& view() { return *mock_view_; }

  TouchToFillController& touch_to_fill_controller() {
    return touch_to_fill_controller_;
  }

 private:
  MockTouchToFillView* mock_view_ = nullptr;
  MockPasswordManagerDriver driver_;
  TouchToFillController touch_to_fill_controller_{nullptr, nullptr};
};

TEST_F(TouchToFillControllerTest, Show_And_Fill) {
  CredentialPair credentials[] = {
      {base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
       GURL(kExampleCom), IsPublicSuffixMatch(false)}};

  EXPECT_CALL(view(),
              Show(Eq(base::ASCIIToUTF16("example.com")), IsOriginSecure(true),
                   ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  // Test that we correctly log the absence of an Android credential.
  base::HistogramTester tester;
  EXPECT_CALL(driver(), FillSuggestion(base::ASCIIToUTF16("alice"),
                                       base::ASCIIToUTF16("p4ssw0rd")));
  EXPECT_CALL(driver(), TouchToFillDismissed);
  touch_to_fill_controller().OnCredentialSelected(credentials[0]);
  tester.ExpectUniqueSample("PasswordManager.FilledCredentialWasFromAndroidApp",
                            false, 1);
}

TEST_F(TouchToFillControllerTest, Show_Insecure_Origin) {
  EXPECT_CALL(driver(), GetLastCommittedURL())
      .WillOnce(ReturnRefOfCopy(GURL("http://example.com")));

  CredentialPair credentials[] = {
      {base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
       GURL(kExampleCom), IsPublicSuffixMatch(false)}};

  EXPECT_CALL(view(),
              Show(Eq(base::ASCIIToUTF16("example.com")), IsOriginSecure(false),
                   ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());
}

TEST_F(TouchToFillControllerTest, Show_And_Fill_Android_Credential) {
  // Test multiple credentials with one of them being an Android credential.
  CredentialPair credentials[] = {
      {base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
       GURL(kExampleCom), IsPublicSuffixMatch(false)},
      {base::ASCIIToUTF16("bob"), base::ASCIIToUTF16("s3cr3t"),
       GURL("android://hash@com.example.my"), IsPublicSuffixMatch(false)}};

  EXPECT_CALL(view(),
              Show(Eq(base::ASCIIToUTF16("example.com")), IsOriginSecure(true),
                   ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  // Test that we correctly log the presence of an Android credential.
  base::HistogramTester tester;
  EXPECT_CALL(driver(), FillSuggestion(base::ASCIIToUTF16("bob"),
                                       base::ASCIIToUTF16("s3cr3t")));
  EXPECT_CALL(driver(), TouchToFillDismissed);
  touch_to_fill_controller().OnCredentialSelected(credentials[1]);
  tester.ExpectUniqueSample("PasswordManager.FilledCredentialWasFromAndroidApp",
                            true, 1);
}

TEST_F(TouchToFillControllerTest, Dismiss) {
  CredentialPair credentials[] = {
      {base::ASCIIToUTF16("alice"), base::ASCIIToUTF16("p4ssw0rd"),
       GURL(kExampleCom), IsPublicSuffixMatch(false)}};

  EXPECT_CALL(view(),
              Show(Eq(base::ASCIIToUTF16("example.com")), IsOriginSecure(true),
                   ElementsAreArray(credentials)));
  touch_to_fill_controller().Show(credentials, driver().AsWeakPtr());

  EXPECT_CALL(driver(), TouchToFillDismissed);
  touch_to_fill_controller().OnDismiss();
}

TEST_F(TouchToFillControllerTest, CanDisableOnHttps) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      autofill::features::kAutofillTouchToFill,
      {
          {"insecure-origins-only", "true"},
      });

  EXPECT_CALL(driver(), TouchToFillDismissed);
  EXPECT_CALL(view(), Show).Times(0);
  touch_to_fill_controller().Show({}, driver().AsWeakPtr());
}
