// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/show_generic_ui_action.h"

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/user_model.h"
#include "components/autofill_assistant/browser/value_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class ShowGenericUiActionTest : public testing::Test {
 public:
  ShowGenericUiActionTest() {}

  void SetUp() override {
    ON_CALL(mock_action_delegate_, OnSetGenericUi(_, _))
        .WillByDefault(Invoke(
            [this](std::unique_ptr<GenericUserInterfaceProto> generic_ui,
                   base::OnceCallback<void(bool, ProcessedActionStatusProto,
                                           const UserModel*)>&
                       end_action_callback) {
              std::move(end_action_callback)
                  .Run(true, UNKNOWN_ACTION_STATUS, &user_model_);
            }));

    ON_CALL(mock_action_delegate_, ClearGenericUi()).WillByDefault(Return());
  }

 protected:
  void Run() {
    // Apply initial model values as specified by proto.
    user_model_.MergeWithProto(proto_.generic_user_interface().model(), false);
    ActionProto action_proto;
    *action_proto.mutable_show_generic_ui() = proto_;
    ShowGenericUiAction action(&mock_action_delegate_, action_proto);
    action.ProcessAction(callback_.Get());
  }

  UserModel user_model_;
  MockActionDelegate mock_action_delegate_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ShowGenericUiProto proto_;
};

TEST_F(ShowGenericUiActionTest, SmokeTest) {
  ON_CALL(mock_action_delegate_, OnSetGenericUi(_, _))
      .WillByDefault(Invoke(
          [this](
              std::unique_ptr<GenericUserInterfaceProto> generic_ui,
              base::OnceCallback<void(bool, ProcessedActionStatusProto,
                                      const UserModel*)>& end_action_callback) {
            std::move(end_action_callback)
                .Run(false, INVALID_ACTION, &user_model_);
          }));

  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));

  Run();
}

TEST_F(ShowGenericUiActionTest, GoesIntoPromptState) {
  ON_CALL(mock_action_delegate_, OnSetGenericUi(_, _))
      .WillByDefault(Invoke(
          [this](
              std::unique_ptr<GenericUserInterfaceProto> generic_ui,
              base::OnceCallback<void(bool, ProcessedActionStatusProto,
                                      const UserModel*)>& end_action_callback) {
            std::move(end_action_callback)
                .Run(true, ACTION_APPLIED, &user_model_);
          }));

  InSequence seq;
  EXPECT_CALL(mock_action_delegate_, Prompt(_, _, _)).Times(1);
  EXPECT_CALL(mock_action_delegate_, OnSetGenericUi(_, _)).Times(1);
  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt()).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));

  Run();
}

TEST_F(ShowGenericUiActionTest, EmptyOutputModel) {
  auto* input_value =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value->set_identifier("input");
  *input_value->mutable_value() = SimpleValue(std::string("InputValue"));

  ON_CALL(mock_action_delegate_, OnSetGenericUi(_, _))
      .WillByDefault(Invoke(
          [this](
              std::unique_ptr<GenericUserInterfaceProto> generic_ui,
              base::OnceCallback<void(bool, ProcessedActionStatusProto,
                                      const UserModel*)>& end_action_callback) {
            std::move(end_action_callback)
                .Run(true, ACTION_APPLIED, &user_model_);
          }));

  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::show_generic_ui_result,
                   Property(&ShowGenericUiProto::Result::model,
                            Property(&ModelProto::values, SizeIs(0))))))));

  Run();
}

TEST_F(ShowGenericUiActionTest, NonEmptyOutputModel) {
  auto* input_value_a =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_a->set_identifier("value_1");
  *input_value_a->mutable_value() = SimpleValue(std::string("input-only"));
  auto* input_value_b =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_b->set_identifier("value_2");
  *input_value_b->mutable_value() = SimpleValue(std::string("Preset value"));

  proto_.add_output_model_identifiers("value_2");

  ON_CALL(mock_action_delegate_, OnSetGenericUi(_, _))
      .WillByDefault(Invoke(
          [this](
              std::unique_ptr<GenericUserInterfaceProto> generic_ui,
              base::OnceCallback<void(bool, ProcessedActionStatusProto,
                                      const UserModel*)>& end_action_callback) {
            user_model_.SetValue("value_2", SimpleValue(std::string("change")));
            std::move(end_action_callback)
                .Run(true, ACTION_APPLIED, &user_model_);
          }));

  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::show_generic_ui_result,
                   Property(&ShowGenericUiProto::Result::model,
                            Property(&ModelProto::values,
                                     UnorderedElementsAre(SimpleModelValue(
                                         "value_2", SimpleValue(std::string(
                                                        "change")))))))))));

  Run();
}

TEST_F(ShowGenericUiActionTest, OutputModelNotSubsetOfInputModel) {
  auto* input_value_a =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_a->set_identifier("value_1");
  *input_value_a->mutable_value() = SimpleValue(std::string("input-only"));
  auto* input_value_b =
      proto_.mutable_generic_user_interface()->mutable_model()->add_values();
  input_value_b->set_identifier("value_2");
  *input_value_b->mutable_value() = SimpleValue(std::string("Preset value"));

  user_model_.SetValue("value_3",
                       SimpleValue(std::string("from_previous_action")));
  proto_.add_output_model_identifiers("value_2");
  proto_.add_output_model_identifiers("value_3");

  EXPECT_CALL(mock_action_delegate_, OnSetGenericUi(_, _)).Times(0);
  EXPECT_CALL(mock_action_delegate_, ClearGenericUi()).Times(1);
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, INVALID_ACTION),
          Property(&ProcessedActionProto::show_generic_ui_result,
                   Property(&ShowGenericUiProto::Result::model,
                            Property(&ModelProto::values, SizeIs(0))))))));

  Run();
}

}  // namespace
}  // namespace autofill_assistant
