// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/basic_interactions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/interactions.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

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
  *proto.mutable_value() = SimpleValue(std::string("success"));

  // Model identifier not set.
  EXPECT_FALSE(basic_interactions_.SetValue(proto));

  proto.set_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.SetValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), proto.value());
}

TEST_F(BasicInteractionsTest, ComputeValueBooleanAnd) {
  ComputeValueProto proto;
  proto.mutable_boolean_and()->add_model_identifiers("value_1");
  proto.mutable_boolean_and()->add_model_identifiers("value_2");
  proto.mutable_boolean_and()->add_model_identifiers("value_3");

  user_model_.SetValue("value_1", SimpleValue(true));
  user_model_.SetValue("value_2", SimpleValue(true));
  user_model_.SetValue("value_3", SimpleValue(false));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  proto.set_result_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(false));

  user_model_.SetValue("value_3", SimpleValue(true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(true));

  // Mixing types is not allowed.
  user_model_.SetValue("value_1", ValueProto());
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  // All input values must have size 1.
  ValueProto multi_bool;
  multi_bool.mutable_booleans()->add_values(true);
  multi_bool.mutable_booleans()->add_values(false);
  user_model_.SetValue("value_2", multi_bool);
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));
}

TEST_F(BasicInteractionsTest, ComputeValueBooleanOr) {
  ComputeValueProto proto;
  proto.mutable_boolean_or()->add_model_identifiers("value_1");
  proto.mutable_boolean_or()->add_model_identifiers("value_2");
  proto.mutable_boolean_or()->add_model_identifiers("value_3");

  user_model_.SetValue("value_1", SimpleValue(false));
  user_model_.SetValue("value_2", SimpleValue(false));
  user_model_.SetValue("value_3", SimpleValue(false));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  proto.set_result_model_identifier("output");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(false));

  user_model_.SetValue("value_2", SimpleValue(true));
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("output"), SimpleValue(true));

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
  proto.mutable_boolean_not()->set_model_identifier("value");
  user_model_.SetValue("value", SimpleValue(false));

  // Result model identifier not set.
  EXPECT_FALSE(basic_interactions_.ComputeValue(proto));

  proto.set_result_model_identifier("value");
  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("value"), SimpleValue(true));

  EXPECT_TRUE(basic_interactions_.ComputeValue(proto));
  EXPECT_EQ(user_model_.GetValue("value"), SimpleValue(false));

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

TEST_F(BasicInteractionsTest, EndActionWithoutCallbackFails) {
  EndActionProto proto;
  ASSERT_DEATH(basic_interactions_.EndAction(proto),
               "Failed to EndAction: no callback set");
}

TEST_F(BasicInteractionsTest, EndActionWithCallbackSucceeds) {
  base::MockCallback<
      base::OnceCallback<void(ProcessedActionStatusProto, const UserModel*)>>
      callback;
  basic_interactions_.SetEndActionCallback(callback.Get());

  EndActionProto proto;
  proto.set_status(ACTION_APPLIED);

  EXPECT_CALL(callback, Run(ACTION_APPLIED, &user_model_));
  EXPECT_TRUE(basic_interactions_.EndAction(proto));
}

}  // namespace autofill_assistant
