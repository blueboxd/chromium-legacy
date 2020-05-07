// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/basic_interactions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/icu_test_util.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/generic_ui.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
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
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(true, true));

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
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(true, true));

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
  EXPECT_EQ(user_model_.GetValue("value"), SimpleValue(true, true));

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
  user_model_.SetValue("value", SimpleValue(1),
                       /* is_client_side_only = */ true);

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

  // Empty value fails.
  user_model_.SetValue("value", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // Multi value fails.
  ValueProto multi_value;
  multi_value.mutable_booleans()->add_values(true);
  multi_value.mutable_booleans()->add_values(false);
  user_model_.SetValue("value", multi_value);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
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
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(7, true));
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
  EXPECT_EQ(user_model_.GetValue("result"), SimpleValue(true, true));
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
