// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/autofill_field_formatter.h"

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace autofill_field_formatter {
namespace {
const char kFakeUrl[] = "https://www.example.com";

using ::testing::_;
using ::testing::Eq;

TEST(AutofillFieldFormatterTest, AutofillProfile) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "John", "", "Doe", "editor@gmail.com", "", "203 Barfield Lane",
      "", "Mountain View", "CA", "94043", "US", "+12345678901");
  // NAME_FIRST
  EXPECT_EQ(*FormatString(profile, "3", "en-US"), "John");
  EXPECT_EQ(*FormatString(profile, "${3}", "en-US"), "John");

  // UNKNOWN_TYPE
  EXPECT_EQ(FormatString(profile, "1", "en-US"), base::nullopt);

  // PHONE_HOME_COUNTRY_CODE, PHONE_HOME_CITY_CODE, PHONE_HOME_NUMBER
  EXPECT_EQ(*FormatString(profile, "(+${12}) (${11}) ${10}", "en-US"),
            "(+1) (234) 5678901");
}

TEST(AutofillFieldFormatterTest, CreditCard) {
  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "John Doe",
                                    "4111 1111 1111 1111", "01", "2050", "");

  // CREDIT_CARD_NAME_FULL
  EXPECT_EQ(*FormatString(credit_card, "51", "en-US"), "John Doe");

  // CREDIT_CARD_NUMBER
  EXPECT_EQ(*FormatString(credit_card, "52", "en-US"), "4111111111111111");

  // CREDIT_CARD_NUMBER_LAST_FOUR_DIGITS
  EXPECT_EQ(*FormatString(credit_card, "**** ${-4}", "en-US"), "**** 1111");

  // CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR
  EXPECT_EQ(*FormatString(credit_card, "${53}/${54}", "en-US"), "01/50");

  // CREDIT_CARD_NETWORK, CREDIT_CARD_NETWORK_FOR_DISPLAY
  EXPECT_EQ(*FormatString(credit_card, "${-2} ${-5}", "en-US"), "visa Visa");
}

TEST(AutofillFieldFormatterTest, SpecialCases) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "John", "", "Doe", "editor@gmail.com", "", "203 Barfield Lane",
      "", "Mountain View", "CA", "94043", "US", "+12345678901");

  EXPECT_EQ(*FormatString(profile, "", "en-US"), std::string());
  EXPECT_EQ(*FormatString(profile, "3", "en-US"), "John");
  EXPECT_EQ(FormatString(profile, "-1", "en-US"), base::nullopt);
  EXPECT_EQ(FormatString(profile,
                         base::NumberToString(autofill::MAX_VALID_FIELD_TYPE),
                         "en-US"),
            base::nullopt);

  // Second {} is not prefixed with $.
  EXPECT_EQ(*FormatString(profile, "${3} {10}", "en-US"), "John {10}");
}

TEST(AutofillFieldFormatterTest, DifferentLocales) {
  autofill::AutofillProfile profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(
      &profile, "John", "", "Doe", "editor@gmail.com", "", "203 Barfield Lane",
      "", "Mountain View", "CA", "94043", "US", "+12345678901");

  // 43 == Country
  EXPECT_EQ(*FormatString(profile, "${43}", "en-US"), "United States");
  EXPECT_EQ(*FormatString(profile, "${43}", "de-DE"), "Vereinigte Staaten");

  // Invalid locales default to "en-US".
  EXPECT_EQ(*FormatString(profile, "43", ""), "United States");
  EXPECT_EQ(*FormatString(profile, "43", "invalid"), "United States");
}

}  // namespace
}  // namespace autofill_field_formatter
}  // namespace autofill_assistant
