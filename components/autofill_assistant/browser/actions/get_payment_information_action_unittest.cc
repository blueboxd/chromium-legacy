// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/get_payment_information_action.h"

#include <utility>

#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"

namespace autofill_assistant {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

class GetPaymentInformationActionTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, GetPaymentInformation(_))
        .WillByDefault(
            Invoke([](std::unique_ptr<PaymentRequestOptions> options) {
              auto payment_information = std::make_unique<PaymentInformation>();
              std::move(options->confirm_callback)
                  .Run(std::move(payment_information));
            }));
  }

 protected:
  base::MockCallback<Action::ProcessActionCallback> callback_;
  MockPersonalDataManager mock_personal_data_manager_;
  MockActionDelegate mock_action_delegate_;
};

TEST_F(GetPaymentInformationActionTest, PromptIsShown) {
  const char kPrompt[] = "Some message.";

  ActionProto action_proto;
  action_proto.mutable_get_payment_information()->set_prompt(kPrompt);
  GetPaymentInformationAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(kPrompt));
  EXPECT_CALL(callback_, Run(_));
  action.ProcessAction(callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
