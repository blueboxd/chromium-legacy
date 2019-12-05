// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/user_data_util.h"

#include "base/strings/string16.h"
#include "base/test/gmock_callback_support.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
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

  std::vector<int> profile_indexes =
      autofill_assistant::SortByCompleteness(options, profiles);
  EXPECT_THAT(profile_indexes, SizeIs(profiles.size()));
  EXPECT_EQ(profile_indexes[0], 2);
  EXPECT_EQ(profile_indexes[1], 1);
  EXPECT_EQ(profile_indexes[2], 0);
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

  std::vector<int> profile_indexes =
      autofill_assistant::SortByCompleteness(options, profiles);
  EXPECT_THAT(profile_indexes, SizeIs(profiles.size()));
  EXPECT_EQ(profile_indexes[0], 2);
  EXPECT_EQ(profile_indexes[1], 1);
  EXPECT_EQ(profile_indexes[2], 0);
}
}  // namespace
}  // namespace autofill_assistant
