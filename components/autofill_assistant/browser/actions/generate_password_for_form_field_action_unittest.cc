// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/generate_password_for_form_field_action.h"

#include <string>
#include <utility>

#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_website_login_manager.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kFakeUrl[] = "https://www.example.com";
const char kFakeSelector[] = "#some_selector";
const char kFakeUsername[] = "user@example.com";
const char kGeneratedPassword[] = "m-W2b-_.7Fu9A.A";
const char kMemoryKeyForGeneratedPassword[] = "memory-key-for-generation";
}  // namespace

namespace autofill_assistant {
using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::Return;

class GeneratePasswordForFormFieldActionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, WriteUserData)
        .WillByDefault(
            RunOnceCallback<0>(&user_data_, /* field_change = */ nullptr));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginManager)
        .WillByDefault(Return(&mock_website_login_manager_));

    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));

    ON_CALL(mock_website_login_manager_, GetGeneratedPassword())
        .WillByDefault(Return(kGeneratedPassword));

    user_data_.selected_login_ =
        base::make_optional<WebsiteLoginManager::Login>(GURL(kFakeUrl),
                                                        kFakeUsername);
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  MockWebsiteLoginManager mock_website_login_manager_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  ActionProto proto_;
  UserData user_data_;
};

TEST_F(GeneratePasswordForFormFieldActionTest, GeneratedPassword) {
  GeneratePasswordForFormFieldProto* generate_password_proto =
      proto_.mutable_generate_password_for_form_field();
  generate_password_proto->mutable_element()->add_selectors(kFakeSelector);
  generate_password_proto->mutable_element()->set_visibility_requirement(
      MUST_BE_VISIBLE);
  generate_password_proto->set_memory_key(kMemoryKeyForGeneratedPassword);

  Selector fake_selector = Selector({kFakeSelector}).MustBeVisible();

  GeneratePasswordForFormFieldAction action(&mock_action_delegate_, proto_);

  EXPECT_CALL(mock_website_login_manager_,
              OnPresaveGeneratedPassword(_, kGeneratedPassword, _, _))
      .Times(1);

  action.ProcessAction(callback_.Get());
  EXPECT_EQ(kGeneratedPassword,
            user_data_.additional_values_[kMemoryKeyForGeneratedPassword]
                .strings()
                .values(0));
}
}  // namespace autofill_assistant
