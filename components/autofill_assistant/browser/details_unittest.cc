// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/details.h"

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/geo/country_names.h"
#include "components/autofill_assistant/browser/details.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_assistant {
namespace {

using ::testing::Eq;

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

class DetailsTest : public testing::Test {
 public:
  DetailsTest() {}

  void SetUp() override { autofill::CountryNames::SetLocaleString("us-en"); }

 protected:
  std::unique_ptr<autofill::CreditCard> MakeCreditCard() {
    return std::make_unique<autofill::CreditCard>();
  }

  std::unique_ptr<autofill::AutofillProfile> MakeAutofillProfile() {
    // The email contains a UTF-8 smiley face.
    auto profile = std::make_unique<autofill::AutofillProfile>();
    autofill::test::SetProfileInfo(profile.get(), "Charles", "Hardin", "Holley",
                                   "\xE2\x98\xBA@gmail.com", "Decca",
                                   "123 Apple St.", "unit 6", "Lubbock",
                                   "Texas", "79401", "US", "23456789012");
    return profile;
  }

  ClientMemory client_memory_;
};

TEST_F(DetailsTest, DetailsProtoStoredInMemberVariable) {
  Details details;
  DetailsProto proto;
  proto.set_title("title");

  details.SetDetailsProto(proto);
  EXPECT_THAT(proto, EqualsProto(details.details_proto()));
}

TEST_F(DetailsTest, DetailsChangesProto) {
  Details details;
  DetailsChangesProto proto;
  proto.set_user_approval_required(true);

  details.SetDetailsChangesProto(proto);
  EXPECT_THAT(proto, EqualsProto(details.changes()));

  details.ClearChanges();
  EXPECT_THAT(DetailsChangesProto(), EqualsProto(details.changes()));
}

TEST_F(DetailsTest, UpdateFromParametersEmpty) {
  Details details;
  // Nothing has to be updated.
  auto context = TriggerContext::CreateEmpty();
  EXPECT_FALSE(details.UpdateFromParameters(*context));
}

TEST_F(DetailsTest, UpdateFromParametersShowInitialNoUpdate) {
  std::map<std::string, std::string> parameters;
  parameters["DETAILS_SHOW_INITIAL"] = "false";
  auto context = TriggerContext::Create(parameters, "exps");

  Details details;
  EXPECT_FALSE(details.UpdateFromParameters(*context));
}

TEST_F(DetailsTest, UpdateFromParametersUpdateFromDetails) {
  std::map<std::string, std::string> parameters;
  parameters["DETAILS_SHOW_INITIAL"] = "true";
  parameters["DETAILS_TITLE"] = "title";
  parameters["DETAILS_DESCRIPTION_LINE_1"] = "line1";
  parameters["DETAILS_DESCRIPTION_LINE_2"] = "line2";
  parameters["DETAILS_DESCRIPTION_LINE_3"] = "line3";
  parameters["DETAILS_IMAGE_URL"] = "image";
  parameters["DETAILS_IMAGE_CLICKTHROUGH_URL"] = "clickthrough";
  parameters["DETAILS_TOTAL_PRICE_LABEL"] = "total";
  parameters["DETAILS_TOTAL_PRICE"] = "12";

  auto context = TriggerContext::Create(parameters, "exps");

  Details details;
  EXPECT_TRUE(details.UpdateFromParameters(*context));

  DetailsProto expected;
  expected.set_animate_placeholders(true);
  expected.set_show_image_placeholder(true);

  expected.set_title("title");
  expected.set_description_line_1("line1");
  expected.set_description_line_2("line2");
  expected.set_description_line_3("line3");
  expected.set_image_url("image");
  auto* data = expected.mutable_image_clickthrough_data();
  data->set_allow_clickthrough(true);
  data->set_clickthrough_url("clickthrough");
  expected.set_total_price_label("total");
  expected.set_total_price("12");

  EXPECT_THAT(details.details_proto(), EqualsProto(expected));
}

TEST_F(DetailsTest, UpdateFromParametersBackwardsCompatibility) {
  std::map<std::string, std::string> parameters;
  parameters["MOVIES_MOVIE_NAME"] = "movie_name";
  parameters["MOVIES_THEATER_NAME"] = "movie_theater";
  parameters["MOVIES_SCREENING_DATETIME"] = "datetime";

  auto context = TriggerContext::Create(parameters, "exps");

  Details details;
  EXPECT_TRUE(details.UpdateFromParameters(*context));

  DetailsProto expected;
  expected.set_animate_placeholders(true);
  expected.set_show_image_placeholder(true);

  expected.set_title("movie_name");
  expected.set_description_line_2("movie_theater");

  EXPECT_THAT(details.datetime(), "datetime");
  EXPECT_THAT(details.details_proto().title(), "movie_name");
  EXPECT_THAT(details.details_proto().description_line_2(), "movie_theater");

  EXPECT_THAT(details.details_proto(), EqualsProto(expected));
}

TEST_F(DetailsTest, UpdateFromProtoNoDetails) {
  Details details;
  EXPECT_FALSE(Details::UpdateFromProto(ShowDetailsProto(), &details));
}

TEST_F(DetailsTest, UpdateFromProto) {
  ShowDetailsProto proto;
  proto.mutable_details()->set_title("title");
  proto.mutable_change_flags()->set_user_approval_required(true);

  Details details;
  EXPECT_TRUE(Details::UpdateFromProto(proto, &details));

  EXPECT_EQ(details.details_proto().title(), "title");
  EXPECT_TRUE(details.changes().user_approval_required());
}

TEST_F(DetailsTest, UpdateFromContactDetailsNoAddressInMemory) {
  EXPECT_FALSE(Details::UpdateFromContactDetails(ShowDetailsProto(),
                                                 &client_memory_, nullptr));
}

TEST_F(DetailsTest, UpdateFromContactDetails) {
  ShowDetailsProto proto;
  proto.set_contact_details("contact");
  client_memory_.set_selected_address("contact", MakeAutofillProfile());

  Details details;
  EXPECT_TRUE(
      Details::UpdateFromContactDetails(proto, &client_memory_, &details));

  const auto& result = details.details_proto();
  EXPECT_THAT(result.title(),
              Eq(l10n_util::GetStringUTF8(IDS_PAYMENTS_CONTACT_DETAILS_LABEL)));
  EXPECT_THAT(result.description_line_1(), Eq("Charles Hardin Holley"));
  EXPECT_THAT(result.description_line_2(), Eq("\xE2\x98\xBA@gmail.com"));
}

TEST_F(DetailsTest, UpdateFromShippingAddressNoAddressInMemory) {
  EXPECT_FALSE(Details::UpdateFromShippingAddress(ShowDetailsProto(),
                                                  &client_memory_, nullptr));
}

TEST_F(DetailsTest, UpdateFromShippingAddress) {
  ShowDetailsProto proto;
  proto.set_shipping_address("shipping");
  client_memory_.set_selected_address("shipping", MakeAutofillProfile());

  Details details;
  EXPECT_TRUE(
      Details::UpdateFromShippingAddress(proto, &client_memory_, &details));

  const auto& result = details.details_proto();
  EXPECT_THAT(
      result.title(),
      Eq(l10n_util::GetStringUTF8(IDS_PAYMENTS_SHIPPING_ADDRESS_LABEL)));
  EXPECT_THAT(result.description_line_1(), Eq("Charles Hardin Holley"));
  EXPECT_THAT(result.description_line_2(),
              Eq("123 Apple St.\nunit 6 79401 Lubbock US"));
}

TEST_F(DetailsTest, UpdateFromSelectedCreditCardEmptyMemory) {
  ShowDetailsProto proto;
  proto.set_credit_card(true);
  EXPECT_FALSE(Details::UpdateFromContactDetails(ShowDetailsProto(),
                                                 &client_memory_, nullptr));
}

TEST_F(DetailsTest, UpdateFromSelectedCreditCardNotRequested) {
  ShowDetailsProto proto;
  proto.set_credit_card(false);
  client_memory_.set_selected_card(MakeCreditCard());
  EXPECT_FALSE(Details::UpdateFromContactDetails(ShowDetailsProto(),
                                                 &client_memory_, nullptr));
}

TEST_F(DetailsTest, UpdateFromCreditCard) {
  ShowDetailsProto proto;
  proto.set_credit_card(true);
  client_memory_.set_selected_card(MakeCreditCard());

  Details details;
  EXPECT_TRUE(
      Details::UpdateFromSelectedCreditCard(proto, &client_memory_, &details));

  const auto& result = details.details_proto();
  EXPECT_THAT(
      result.title(),
      Eq(l10n_util::GetStringUTF8(IDS_PAYMENTS_METHOD_OF_PAYMENT_LABEL)));
  // The credit card string contains 4 non-ascii dots, we just check that it
  // does contain something.
  EXPECT_FALSE(result.description_line_1().empty());
}

}  // namespace
}  // namespace autofill_assistant
