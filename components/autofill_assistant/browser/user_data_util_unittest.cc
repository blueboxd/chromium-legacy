// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::SizeIs;

TEST(UserDataUtilTest, SortsCompleteProfilesAlphabetically) {
  auto profile_a = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_a.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  auto profile_b = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_b.get(), "Berta", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  auto profile_unicode = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_unicode.get(),
                                 "\xC3\x85"
                                 "dam",
                                 "", "West", "aedam.west@gmail.com", "", "", "",
                                 "", "", "", "", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_unicode));
  profiles.emplace_back(std::move(profile_b));
  profiles.emplace_back(std::move(profile_a));

  CollectUserDataOptions options;
  options.request_payer_name = true;
  options.request_payer_email = true;

  std::vector<int> profile_indices =
      autofill_assistant::SortByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(2, 1, 0));
}

TEST(UserDataUtilTest, SortsProfilesByCompleteness) {
  auto profile_complete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(
      profile_complete.get(), "Charlie", "", "West", "charlie.west@gmail.com",
      "", "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "+44");

  auto profile_no_phone = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(
      profile_no_phone.get(), "Berta", "", "West", "berta.west@gmail.com", "",
      "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "");

  auto profile_incomplete = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(profile_incomplete.get(), "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  // Specify profiles in reverse order to force sorting.
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles;
  profiles.emplace_back(std::move(profile_incomplete));
  profiles.emplace_back(std::move(profile_no_phone));
  profiles.emplace_back(std::move(profile_complete));

  CollectUserDataOptions options;
  options.request_payer_name = true;
  options.request_payer_email = true;
  options.request_payer_phone = true;
  options.request_shipping = true;

  std::vector<int> profile_indices =
      autofill_assistant::SortByCompleteness(options, profiles);
  EXPECT_THAT(profile_indices, SizeIs(profiles.size()));
  EXPECT_THAT(profile_indices, ElementsAre(2, 1, 0));
}

TEST(UserDataUtilTest, SortsCreditCardsByCompleteness) {
  auto complete_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(complete_card.get(), "Berta West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto complete_instrument =
      std::make_unique<PaymentInstrument>(std::move(complete_card), nullptr);

  auto incomplete_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(incomplete_card.get(), "Adam West",
                                    "4111111111111111", "", "",
                                    /* billing_address_id= */ "");
  auto incomplete_instrument =
      std::make_unique<PaymentInstrument>(std::move(incomplete_card), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(incomplete_instrument));
  payment_instruments.emplace_back(std::move(complete_instrument));

  CollectUserDataOptions options;

  std::vector<int> sorted_indices =
      SortByCompleteness(options, payment_instruments);
  EXPECT_THAT(sorted_indices, SizeIs(payment_instruments.size()));
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsCompleteCardsByName) {
  auto a_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(a_card.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto a_instrument =
      std::make_unique<PaymentInstrument>(std::move(a_card), nullptr);

  auto b_card = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(b_card.get(), "Berta West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto b_instrument =
      std::make_unique<PaymentInstrument>(std::move(b_card), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(b_instrument));
  payment_instruments.emplace_back(std::move(a_instrument));

  CollectUserDataOptions options;

  std::vector<int> sorted_indices =
      SortByCompleteness(options, payment_instruments);
  EXPECT_THAT(sorted_indices, SizeIs(payment_instruments.size()));
  EXPECT_THAT(sorted_indices, ElementsAre(1, 0));
}

TEST(UserDataUtilTest, SortsCreditCardsByAddressCompleteness) {
  auto card_with_complete_address = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(card_with_complete_address.get(),
                                    "Charlie West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "address-1");
  auto billing_address_with_zip = std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(billing_address_with_zip.get(), "Charlie", "",
                                 "West", "charlie.west@gmail.com", "",
                                 "Baker Street 221b", "", "London", "",
                                 "WC2N 5DU", "UK", "+44");
  auto instrument_with_complete_address =
      std::make_unique<PaymentInstrument>(std::move(card_with_complete_address),
                                          std::move(billing_address_with_zip));

  auto card_with_incomplete_address = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(card_with_incomplete_address.get(),
                                    "Berta West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "address-1");
  auto billing_address_without_zip =
      std::make_unique<autofill::AutofillProfile>();
  autofill::test::SetProfileInfo(billing_address_without_zip.get(), "Berta", "",
                                 "West", "berta.west@gmail.com", "",
                                 "Baker Street 221b", "", "London", "", "",
                                 "UK", "+44");
  auto instrument_with_incomplete_address = std::make_unique<PaymentInstrument>(
      std::move(card_with_incomplete_address),
      std::move(billing_address_without_zip));

  auto card_without_address = std::make_unique<autofill::CreditCard>();
  autofill::test::SetCreditCardInfo(card_without_address.get(), "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  auto instrument_without_address = std::make_unique<PaymentInstrument>(
      std::move(card_without_address), nullptr);

  // Specify payment instruments in reverse order to force sorting.
  std::vector<std::unique_ptr<PaymentInstrument>> payment_instruments;
  payment_instruments.emplace_back(std::move(instrument_without_address));
  payment_instruments.emplace_back(
      std::move(instrument_with_incomplete_address));
  payment_instruments.emplace_back(std::move(instrument_with_complete_address));

  CollectUserDataOptions options;
  options.require_billing_postal_code = true;

  std::vector<int> sorted_indices =
      SortByCompleteness(options, payment_instruments);
  EXPECT_THAT(sorted_indices, SizeIs(payment_instruments.size()));
  EXPECT_THAT(sorted_indices, ElementsAre(2, 1, 0));
}

}  // namespace
}  // namespace autofill_assistant
