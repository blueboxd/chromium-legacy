// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/basic_interactions.h"
#include "base/guid.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/icu_test_util.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Property;
using ::testing::StrEq;
namespace {
DateProto CreateDateProto(int year, int month, int day) {
  DateProto proto;
  proto.set_year(year);
  proto.set_month(month);
  proto.set_day(day);
  return proto;
}
}  // namespace

class BasicInteractionsTest : public testing::Test {
 protected:
  BasicInteractionsTest() { delegate_.SetUserModel(&user_model_); }
  ~BasicInteractionsTest() override {}

  FakeScriptExecutorDelegate delegate_;
  UserModel user_model_;
  BasicInteractions basic_interactions_{&delegate_};
};

TEST_F(BasicInteractionsTest, SetValue) {
  SetModelValueProto proto;
  *proto.mutable_value()->mutable_value() = SimpleValue(std::string("success"));

  // Model identifier not set.
  EXPECT_FALSE(basic_interactions_.SetValue(proto));

  proto.set_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.SetValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), proto.value().value());
}

TEST_F(BasicInteractionsTest, ComputeValueBooleanAnd) {
  ComputeValueProto proto;
  proto.mutable_boolean_and()->add_values()->set_model_identifier("value_1");
  *proto.mutable_boolean_and()->add_values()->mutable_value() =
      SimpleValue(true);
  proto.mutable_boolean_and()->add_values()->set_model_identifier("value_3");

  user_model_.SetValue("value_1", SimpleValue(true));
  user_model_.SetValue("value_3", SimpleValue(false));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  proto.set_result_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(false));

  user_model_.SetValue("value_3", SimpleValue(true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(true));

  // is_client_side_only flag is sticky.
  user_model_.SetValue("value_3",
                       SimpleValue(true, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_TRUE(user_model_.GetValue("output")->is_client_side_only());

  // Mixing types is not allowed.
  user_model_.SetValue("value_1", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // All input values must have size 1.
  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("value_3", multi_bool);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
}

TEST_F(BasicInteractionsTest, ComputeValueBooleanOr) {
  ComputeValueProto proto;
  proto.mutable_boolean_or()->add_values()->set_model_identifier("value_1");
  proto.mutable_boolean_or()->add_values()->set_model_identifier("value_2");
  *proto.mutable_boolean_or()->add_values()->mutable_value() =
      SimpleValue(false);

  user_model_.SetValue("value_1", SimpleValue(false));
  user_model_.SetValue("value_2", SimpleValue(false));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  proto.set_result_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(false));

  user_model_.SetValue("value_2", SimpleValue(true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(true));

  // is_client_side_only flag is sticky.
  user_model_.SetValue("value_2",
                       SimpleValue(true, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_TRUE(user_model_.GetValue("output")->is_client_side_only());

  // Mixing types is not allowed.
  user_model_.SetValue("value_1", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // All input values must have size 1.
  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("value_1", multi_bool);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
}

TEST_F(BasicInteractionsTest, ComputeValueBooleanNot) {
  ComputeValueProto proto;
  proto.mutable_boolean_not()->mutable_value()->set_model_identifier("value");
  user_model_.SetValue("value", SimpleValue(false));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  proto.set_result_model_identifier("value");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("value"), SimpleValue(true));

  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("value"), SimpleValue(false));

  // is_client_side_only flag is sticky.
  user_model_.SetValue("value",
                       SimpleValue(false, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_TRUE(user_model_.GetValue("value")->is_client_side_only());

  // Not a boolean.
  user_model_.SetValue("value", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Size != 1.
  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("value", multi_bool);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
}

TEST_F(BasicInteractionsTest, ComputeValueToString) {
  ComputeValueProto proto;
  proto.mutable_to_string()->mutable_value()->set_model_identifier("value");
  user_model_.SetValue("value", SimpleValue(1,
                                            /* is_client_side_only = */ true));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Integer
  proto.set_result_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(*user_model_.GetValue("output"),
            SimpleValue(std::string("1"), /* is_client_side_only = */ true));

  // Boolean
  user_model_.SetValue("value",
                       SimpleValue(true, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(*user_model_.GetValue("output"),
            SimpleValue(std::string("true"), /* is_client_side_only = */ true));

  // String
  user_model_.SetValue("value", SimpleValue(std::string("test asd"),
                                            /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(
      *user_model_.GetValue("output"),
      SimpleValue(std::string("test asd"), /* is_client_side_only = */ true));

  // Date without format fails.
  user_model_.SetValue("value", SimpleValue(CreateDateProto(2020, 10, 23),
                                            /* is_client_side_only = */ true));
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Date with format succeeds.
  {
    base::test::ScopedRestoreICUDefaultLocale locale(std::string("en_US"));
    proto.mutable_to_string()->mutable_date_format()->set_date_format(
        "EEE, MMM d y");
    EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
    EXPECT_EQ(*user_model_.GetValue("output"),
              SimpleValue(std::string("Fri, Oct 23, 2020"),
                          /* is_client_side_only = */ true));
  }

  // Date in german locale.
  {
    base::test::ScopedRestoreICUDefaultLocale locale(std::string("de_DE"));
    EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
    EXPECT_EQ(*user_model_.GetValue("output"),
              SimpleValue(std::string("Fr., 23. Okt. 2020"),
                          /* is_client_side_only = */ true));
  }

  // Credit cards
  autofill::CreditCard credit_card_a(base::GenerateGUID(),
                                     "https://www.example.com");
  autofill::test::SetCreditCardInfo(&credit_card_a, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050", "");
  autofill::CreditCard credit_card_b(base::GenerateGUID(),
                                     "https://www.example.com");
  autofill::test::SetCreditCardInfo(&credit_card_b, "John Doe",
                                    "4111 1111 1111 2222", "02", "2051", "");
  auto credit_cards =
      std::make_unique<std::vector<std::unique_ptr<autofill::CreditCard>>>();
  credit_cards->emplace_back(
      std::make_unique<autofill::CreditCard>(credit_card_a));
  credit_cards->emplace_back(
      std::make_unique<autofill::CreditCard>(credit_card_b));
  user_model_.SetAutofillCreditCards(std::move(credit_cards));
  ValueProto credit_cards_value;
  credit_cards_value.mutable_credit_cards()->add_values()->set_guid(
      credit_card_a.guid());
  credit_cards_value.mutable_credit_cards()->add_values()->set_guid(
      credit_card_b.guid());
  user_model_.SetValue("value", credit_cards_value);
  // Formatting credit cards fails if pattern or locale are not set.
  proto.mutable_to_string()->mutable_autofill_format()->set_locale("en-US");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  // {name} {network} **** {last-4-digits} ({month/year})
  proto.mutable_to_string()->mutable_autofill_format()->set_pattern(
      "${51}. ${-5} **** ${-4} (${53}/${54})");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  ValueProto expected_result;
  expected_result.mutable_strings()->add_values(
      "Marion Mitchell. Visa **** 1111 (01/50)");
  expected_result.mutable_strings()->add_values(
      "John Doe. Visa **** 2222 (02/51)");
  EXPECT_EQ(*user_model_.GetValue("output"), expected_result);

  // Profiles
  autofill::AutofillProfile profile_a(base::GenerateGUID(),
                                      "https://www.example.com");
  autofill::test::SetProfileInfo(
      &profile_a, "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
      "123 Zoo St.", "unit 5", "Hollywood", "CA", "91601", "US", "16505678910");
  autofill::AutofillProfile profile_b(base::GenerateGUID(),
                                      "https://www.example.com");
  autofill::test::SetProfileInfo(&profile_b, "John", "", "Doe",
                                 "editor@gmail.com", "", "203 Barfield Lane",
                                 "apt A", "Mountain View", "CA", "94043", "US",
                                 "+12345678901");
  auto profiles = std::make_unique<
      std::vector<std::unique_ptr<autofill::AutofillProfile>>>();
  profiles->emplace_back(
      std::make_unique<autofill::AutofillProfile>(profile_a));
  profiles->emplace_back(
      std::make_unique<autofill::AutofillProfile>(profile_b));
  user_model_.SetAutofillProfiles(std::move(profiles));
  ValueProto profiles_value;
  profiles_value.mutable_profiles()->add_values()->set_guid(profile_a.guid());
  profiles_value.mutable_profiles()->add_values()->set_guid(profile_b.guid());
  user_model_.SetValue("value", profiles_value);
  // Formatting profiles fails if pattern or locale are not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  // {name_full}, {address_line_1} {address_line_2} {zip code} {city} {country}
  proto.mutable_to_string()->mutable_autofill_format()->set_pattern(
      "${7} ${30} ${31} ${42} ${40} ${43}");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  expected_result.Clear();
  expected_result.mutable_strings()->add_values(
      "Marion Mitchell Morrison 123 Zoo St. unit 5 91601 Hollywood United "
      "States");
  expected_result.mutable_strings()->add_values(
      "John Doe 203 Barfield Lane apt A 94043 Mountain View United States");
  EXPECT_EQ(*user_model_.GetValue("output"), expected_result);

  // Different locale.
  proto.mutable_to_string()->mutable_autofill_format()->set_locale("de-DE");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  expected_result.Clear();
  expected_result.mutable_strings()->add_values(
      "Marion Mitchell Morrison 123 Zoo St. unit 5 91601 Hollywood Vereinigte "
      "Staaten");
  expected_result.mutable_strings()->add_values(
      "John Doe 203 Barfield Lane apt A 94043 Mountain View Vereinigte "
      "Staaten");
  EXPECT_EQ(*user_model_.GetValue("output"), expected_result);

  // Empty value fails.
  user_model_.SetValue("value", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Multi value succeeds.
  ValueProto multi_value;
  multi_value.mutable_booleans()->add_values(true);
  multi_value.mutable_booleans()->add_values(false);
  user_model_.SetValue("value", multi_value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  expected_result.Clear();
  expected_result.mutable_strings()->add_values("true");
  expected_result.mutable_strings()->add_values("false");
  EXPECT_EQ(*user_model_.GetValue("output"), expected_result);

  multi_value.Clear();
  multi_value.mutable_ints()->add_values(5);
  multi_value.mutable_ints()->add_values(13);
  user_model_.SetValue("value", multi_value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  expected_result.Clear();
  expected_result.mutable_strings()->add_values("5");
  expected_result.mutable_strings()->add_values("13");
  EXPECT_EQ(*user_model_.GetValue("output"), expected_result);

  // is_client_side_only flag is sticky.
  multi_value.set_is_client_side_only(true);
  user_model_.SetValue("value", multi_value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_TRUE(user_model_.GetValue("output")->is_client_side_only());
}

TEST_F(BasicInteractionsTest, ComputeValueIntegerSum) {
  ComputeValueProto proto;
  proto.mutable_integer_sum();
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(2));

  // Missing fields.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_integer_sum()->add_values()->set_model_identifier("value_a");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_integer_sum()->add_values()->set_model_identifier("value_b");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.set_result_model_identifier("result");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));

  // Size != 1.
  ValueProto value;
  value.mutable_ints()->add_values(1);
  value.mutable_ints()->add_values(2);
  user_model_.SetValue("value_a", value);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Check results.
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(2));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(3));

  user_model_.SetValue("value_a", SimpleValue(-1));
  user_model_.SetValue("value_b", SimpleValue(5));
  *proto.mutable_integer_sum()->add_values()->mutable_value() = SimpleValue(3);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(7));

  // is_client_side_only flag is sticky.
  user_model_.SetValue("value_b",
                       SimpleValue(5, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_TRUE(user_model_.GetValue("result")->is_client_side_only());
}

TEST_F(BasicInteractionsTest, EndActionWithoutCallbackFails) {
  EndActionProto proto;
  EXPECT_FALSE(basic_interactions_.EndAction(true, proto));
}

TEST_F(BasicInteractionsTest, EndActionWithCallbackSucceeds) {
  base::MockCallback<base::OnceCallback<void(bool, ProcessedActionStatusProto,
                                             const UserModel*)>>
      callback;
  basic_interactions_.SetEndActionCallback(callback.Get());

  EndActionProto proto;
  proto.set_status(ACTION_APPLIED);

  EXPECT_CALL(callback, Run(true, ACTION_APPLIED, &user_model_));
  EXPECT_TRUE(basic_interactions_.EndAction(true, proto));
}

TEST_F(BasicInteractionsTest, ComputeValueCompare) {
  user_model_.SetValue("value_a", ValueProto());
  user_model_.SetValue("value_b", ValueProto());

  ComputeValueProto proto;
  proto.mutable_comparison();

  // Fields are missing.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_comparison()->mutable_value_a()->set_model_identifier(
      "value_a");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_comparison()->mutable_value_b()->set_model_identifier(
      "value_b");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.set_result_model_identifier("result");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // EQUAL supported for all value types.
  proto.mutable_comparison()->set_mode(ValueComparisonProto::EQUAL);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  user_model_.SetValue("value_a", SimpleValue(std::string("string_a")));
  user_model_.SetValue("value_b", SimpleValue(std::string("string_b")));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  user_model_.SetValue("value_a", SimpleValue(true));
  user_model_.SetValue("value_b", SimpleValue(false));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(2));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  user_model_.SetValue("value_a", SimpleValue(CreateDateProto(2020, 8, 7)));
  user_model_.SetValue("value_b", SimpleValue(CreateDateProto(2020, 11, 5)));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  ValueProto user_actions_value;
  user_actions_value.mutable_user_actions();
  user_model_.SetValue("value_a", user_actions_value);
  user_model_.SetValue("value_b", user_actions_value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));

  // Some types are not supported for comparison mode != EQUAL.
  proto.mutable_comparison()->set_mode(ValueComparisonProto::LESS);
  user_model_.SetValue("value_a", ValueProto());
  user_model_.SetValue("value_b", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  user_model_.SetValue("value_a", SimpleValue(true));
  user_model_.SetValue("value_b", SimpleValue(false));
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  user_model_.SetValue("value_a", user_actions_value);
  user_model_.SetValue("value_b", user_actions_value);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Different types fail for mode != EQUAL.
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(std::string("a")));
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Size != 1 fails for mode != EQUAL.
  ValueProto multi_value;
  multi_value.mutable_booleans()->add_values(true);
  multi_value.mutable_booleans()->add_values(false);
  user_model_.SetValue("value_a", multi_value);
  user_model_.SetValue("value_b", multi_value);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Check comparison results.
  proto.mutable_comparison()->set_mode(ValueComparisonProto::LESS);
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(2));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(1));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(false));

  proto.mutable_comparison()->set_mode(ValueComparisonProto::LESS_OR_EQUAL);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  proto.mutable_comparison()->set_mode(ValueComparisonProto::GREATER_OR_EQUAL);
  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(2));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(false));

  user_model_.SetValue("value_a", SimpleValue(1));
  user_model_.SetValue("value_b", SimpleValue(1));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  proto.mutable_comparison()->set_mode(ValueComparisonProto::GREATER);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(false));

  user_model_.SetValue("value_a", SimpleValue(2));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true));

  proto.mutable_comparison()->set_mode(ValueComparisonProto::GREATER_OR_EQUAL);
  user_model_.SetValue("value_a",
                       SimpleValue(1, /* is_client_side_only = */ false));
  user_model_.SetValue("value_b",
                       SimpleValue(1, /* is_client_side_only = */ true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(*user_model_.GetValue("result"), SimpleValue(true, true));
}

TEST_F(BasicInteractionsTest, ComputeValueCreateCreditCardResponse) {
  ComputeValueProto proto;
  proto.mutable_create_credit_card_response();

  // Missing fields.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_create_credit_card_response()
      ->mutable_value()
      ->set_model_identifier("value");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.set_result_model_identifier("result");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  autofill::CreditCard credit_card(base::GenerateGUID(),
                                   "https://www.example.com");
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2050", "");
  auto credit_cards =
      std::make_unique<std::vector<std::unique_ptr<autofill::CreditCard>>>();
  credit_cards->emplace_back(
      std::make_unique<autofill::CreditCard>(credit_card));
  user_model_.SetAutofillCreditCards(std::move(credit_cards));

  ValueProto value_wrong_guid;
  value_wrong_guid.mutable_credit_cards()->add_values()->set_guid("wrong");
  user_model_.SetValue("value", value_wrong_guid);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  ValueProto value_correct_guid;
  value_correct_guid.mutable_credit_cards()->add_values()->set_guid(
      credit_card.guid());
  user_model_.SetValue("value", value_correct_guid);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  ValueProto expected_response_value;
  expected_response_value.mutable_credit_card_response()->set_network("visa");
  expected_response_value.set_is_client_side_only(false);
  EXPECT_EQ(user_model_.GetValue("result"), expected_response_value);

  // CreateCreditCardResponse is allowed to extract the card network from
  // client-only values.
  value_correct_guid.set_is_client_side_only(true);
  user_model_.SetValue("value", value_correct_guid);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("result"), expected_response_value);

  // Size != 1.
  ValueProto value;
  value.mutable_credit_cards()->add_values()->set_guid(credit_card.guid());
  value.mutable_credit_cards()->add_values()->set_guid(credit_card.guid());
  user_model_.SetValue("value", value);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
}

TEST_F(BasicInteractionsTest, ComputeValueCreateLoginOptionResponse) {
  ComputeValueProto proto;
  proto.mutable_create_login_option_response();

  // Missing fields.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.mutable_create_login_option_response()
      ->mutable_value()
      ->set_model_identifier("value");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
  proto.set_result_model_identifier("result");
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  ValueProto value;
  value.mutable_login_options()->add_values()->set_payload("payload");
  value.set_is_client_side_only(true);
  user_model_.SetValue("value", value);
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));

  // LoginOptionResponseProto is allowed to extract the payload from
  // client-only values.
  ValueProto expected_response_value;
  expected_response_value.mutable_login_option_response()->set_payload(
      "payload");
  expected_response_value.set_is_client_side_only(false);
  EXPECT_EQ(user_model_.GetValue("result"), expected_response_value);
}

TEST_F(BasicInteractionsTest, ToggleUserAction) {
  ToggleUserActionProto proto;
  ValueProto user_actions_value;
  UserActionProto cancel_user_action;
  cancel_user_action.set_identifier("cancel_identifier");
  cancel_user_action.mutable_chip()->set_type(ChipType::CANCEL_ACTION);
  cancel_user_action.mutable_chip()->set_text("Cancel");
  *user_actions_value.mutable_user_actions()->add_values() = cancel_user_action;
  UserActionProto done_user_action;
  done_user_action.set_identifier("done_identifier");
  done_user_action.mutable_chip()->set_type(ChipType::HIGHLIGHTED_ACTION);
  done_user_action.mutable_chip()->set_text("Done");
  *user_actions_value.mutable_user_actions()->add_values() = done_user_action;
  user_model_.SetValue("chips", user_actions_value);
  user_model_.SetValue("enabled", SimpleValue(false));

  // Missing fields.
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  proto.set_user_actions_model_identifier("chips");
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  proto.mutable_enabled()->set_model_identifier("enabled");
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  proto.set_user_action_identifier("done_identifier");
  EXPECT_TRUE(basic_interactions_.ToggleUserAction(proto));

  // Wrong value types/sizes.
  user_model_.SetValue("chips", SimpleValue(3));
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  user_model_.SetValue("chips", user_actions_value);
  user_model_.SetValue("enabled", SimpleValue(2));
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("enabled", multi_bool);
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  user_model_.SetValue("enabled", SimpleValue(false));
  EXPECT_TRUE(basic_interactions_.ToggleUserAction(proto));

  // Wrong user action identifier.
  proto.set_user_action_identifier("wrong");
  EXPECT_FALSE(basic_interactions_.ToggleUserAction(proto));
  proto.set_user_action_identifier("cancel_identifier");
  EXPECT_TRUE(basic_interactions_.ToggleUserAction(proto));

  // Check changes to values.
  user_model_.SetValue("chips", user_actions_value);
  user_model_.SetValue("enabled", SimpleValue(false));
  proto.set_user_action_identifier("done_identifier");
  EXPECT_THAT(
      user_model_.GetValue("chips")->user_actions().values(),
      ElementsAre(AllOf(Property(&UserActionProto::identifier,
                                 StrEq("cancel_identifier")),
                        Property(&UserActionProto::enabled, Eq(true))),
                  AllOf(Property(&UserActionProto::identifier,
                                 StrEq("done_identifier")),
                        Property(&UserActionProto::enabled, Eq(true)))));
  EXPECT_TRUE(basic_interactions_.ToggleUserAction(proto));
  EXPECT_THAT(
      user_model_.GetValue("chips")->user_actions().values(),
      ElementsAre(AllOf(Property(&UserActionProto::identifier,
                                 StrEq("cancel_identifier")),
                        Property(&UserActionProto::enabled, Eq(true))),
                  AllOf(Property(&UserActionProto::identifier,
                                 StrEq("done_identifier")),
                        Property(&UserActionProto::enabled, Eq(false)))));
}

TEST_F(BasicInteractionsTest, RunConditionalCallback) {
  InSequence seq;
  base::MockCallback<base::RepeatingCallback<void()>> callback;

  EXPECT_CALL(callback, Run()).Times(0);
  EXPECT_FALSE(
      basic_interactions_.RunConditionalCallback("condition", callback.Get()));

  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("condition", multi_bool);
  EXPECT_FALSE(
      basic_interactions_.RunConditionalCallback("condition", callback.Get()));

  user_model_.SetValue("condition", SimpleValue(false));
  EXPECT_TRUE(
      basic_interactions_.RunConditionalCallback("condition", callback.Get()));

  EXPECT_CALL(callback, Run()).Times(1);
  user_model_.SetValue("condition", SimpleValue(true));
  EXPECT_TRUE(
      basic_interactions_.RunConditionalCallback("condition", callback.Get()));
}

}  // namespace autofill_assistant
