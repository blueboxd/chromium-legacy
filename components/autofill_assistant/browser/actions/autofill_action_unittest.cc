// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/autofill_action.h"

#include <utility>

#include "base/bind.h"
#include "base/guid.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Expectation;
using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::Not;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::StrNe;

class MockPersonalDataManager : public autofill::PersonalDataManager {
 public:
  MockPersonalDataManager() : PersonalDataManager("en-US") {}
  ~MockPersonalDataManager() override {}

  // PersonalDataManager:
  std::string SaveImportedProfile(
      const autofill::AutofillProfile& profile) override {
    std::vector<autofill::AutofillProfile> profiles;
    std::string merged_guid =
        MergeProfile(profile, &profiles_, "en-US", &profiles);
    if (merged_guid == profile.guid())
      profiles_.push_back(std::make_unique<autofill::AutofillProfile>(profile));
    return merged_guid;
  }

  autofill::AutofillProfile* GetProfileByGUID(
      const std::string& guid) override {
    autofill::AutofillProfile* result = nullptr;
    for (const auto& profile : profiles_) {
      if (profile->guid() != guid)
        continue;
      result = profile.get();
      break;
    }

    return result;
  }

 private:
  std::vector<std::unique_ptr<autofill::AutofillProfile>> profiles_;

  DISALLOW_COPY_AND_ASSIGN(MockPersonalDataManager);
};

// A callback that expects to be called immediately.
//
// This relies on mocked methods calling their callbacks immediately (which is
// the case in this test).
class DirectCallback {
 public:
  DirectCallback() : was_run_(false), result_(nullptr) {}

  // Returns a base::OnceCallback. The current instance must exist until
  // GetResultOrDie is called.
  base::OnceCallback<void(std::unique_ptr<ProcessedActionProto>)> Get() {
    return base::BindOnce(&DirectCallback::Run, base::Unretained(this));
  }

  ProcessedActionProto* GetResultOrDie() {
    CHECK(was_run_);
    return result_.get();
  }

 private:
  void Run(std::unique_ptr<ProcessedActionProto> result) {
    was_run_ = true;
    result_ = std::move(result);
  }

  bool was_run_;
  std::unique_ptr<ProcessedActionProto> result_;
};

class AutofillActionTest : public testing::Test {
 public:
  void SetUp() override {
    auto autofill_profile = std::make_unique<autofill::AutofillProfile>(
        base::GenerateGUID(), autofill::test::kEmptyOrigin);
    autofill::test::SetProfileInfo(autofill_profile.get(), kFirstName, "",
                                   kLastName, kEmail, "", "", "", "", "", "",
                                   "", "");
    personal_data_manager_ = std::make_unique<MockPersonalDataManager>();
    personal_data_manager_->SaveImportedProfile(*autofill_profile);

    client_memory_.set_selected_address(kAddressName,
                                        std::move(autofill_profile));

    ON_CALL(mock_action_delegate_, GetClientMemory)
        .WillByDefault(Return(&client_memory_));
    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(personal_data_manager_.get()));
    ON_CALL(mock_action_delegate_, RunElementChecks)
        .WillByDefault(Invoke([this](BatchElementChecker* checker) {
          checker->Run(&mock_web_controller_);
        }));
    ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
        .WillByDefault(RunOnceCallback<1>(true));
  }

 protected:
  const char* const kAddressName = "billing";
  const char* const kFakeSelector = "#selector";
  const char* const kSelectionPrompt = "prompt";
  const char* const kFirstName = "FirstName";
  const char* const kLastName = "LastName";
  const char* const kEmail = "foobar@gmail.com";

  ActionProto CreateUseAddressAction() {
    ActionProto action;
    UseAddressProto* use_address = action.mutable_use_address();
    use_address->set_name(kAddressName);
    use_address->mutable_form_field_element()->add_selectors(kFakeSelector);
    return action;
  }

  ActionProto CreateUseCreditCardAction() {
    ActionProto action;
    action.mutable_use_card()->mutable_form_field_element()->add_selectors(
        kFakeSelector);
    return action;
  }

  UseAddressProto::RequiredField* AddRequiredField(
      ActionProto* action,
      UseAddressProto::RequiredField::AddressField type,
      std::string selector) {
    auto* required_field = action->mutable_use_address()->add_required_fields();
    required_field->set_address_field(type);
    required_field->mutable_element()->add_selectors(selector);
    return required_field;
  }

  UseCreditCardProto::RequiredField* AddRequiredField(
      ActionProto* action,
      UseCreditCardProto::RequiredField::CardField type,
      std::string selector) {
    auto* required_field = action->mutable_use_card()->add_required_fields();
    required_field->set_card_field(type);
    required_field->mutable_element()->add_selectors(selector);
    return required_field;
  }

  ActionProto CreateUseCardAction() {
    ActionProto action;
    UseCreditCardProto* use_card = action.mutable_use_card();
    use_card->mutable_form_field_element()->add_selectors(kFakeSelector);
    return action;
  }

  ProcessedActionStatusProto ProcessAction(const ActionProto& action_proto) {
    AutofillAction action(&mock_action_delegate_, action_proto);
    // We can use DirectCallback given that methods in ActionDelegate are mocked
    // and return directly.
    DirectCallback callback;
    action.ProcessAction(callback.Get());
    return callback.GetResultOrDie()->status();
  }

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  ClientMemory client_memory_;
  std::unique_ptr<autofill::PersonalDataManager> personal_data_manager_;
};

#if !defined(OS_ANDROID)
#define MAYBE_FillManually FillManually
#else
#define MAYBE_FillManually DISABLED_FillManually
#endif
TEST_F(AutofillActionTest, MAYBE_FillManually) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  action_proto.mutable_use_address()->set_prompt(kSelectionPrompt);

  EXPECT_EQ(ProcessedActionStatusProto::MANUAL_FALLBACK,
            ProcessAction(action_proto));
}

TEST_F(AutofillActionTest, NoSelectedAddress) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  action_proto.mutable_use_address()->set_prompt(kSelectionPrompt);

  client_memory_.set_selected_address(kAddressName, nullptr);

  EXPECT_EQ(ProcessedActionStatusProto::PRECONDITION_FAILED,
            ProcessAction(action_proto));
}

TEST_F(AutofillActionTest, PreconditionFailedPopulatesUnexpectedErrorInfo) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  action_proto.mutable_use_address()->set_prompt(kSelectionPrompt);
  client_memory_.set_selected_address(kAddressName, nullptr);
  client_memory_.set_selected_address("one_more", nullptr);

  AutofillAction action(&mock_action_delegate_, action_proto);

  // We can use DirectCallback given that methods in ActionDelegate are mocked
  // and return directly.
  DirectCallback callback;
  action.ProcessAction(callback.Get());

  auto* processed_action = callback.GetResultOrDie();
  EXPECT_EQ(ProcessedActionStatusProto::PRECONDITION_FAILED,
            processed_action->status());
  const auto& error_info =
      processed_action->status_details().autofill_error_info();
  EXPECT_EQ(base::JoinString({kAddressName, "one_more"}, ","),
            error_info.client_memory_address_key_names());
  EXPECT_EQ(kAddressName, error_info.address_key_requested());
}

TEST_F(AutofillActionTest, ShortWaitForElementVisible) {
  EXPECT_CALL(
      mock_action_delegate_,
      OnShortWaitForElement(Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<1>(true));

  ActionProto action_proto = CreateUseAddressAction();
  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_, OnFillAddressForm(NotNull(), _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Validation succeeds.
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(true, "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED,
            ProcessAction(action_proto));
}

TEST_F(AutofillActionTest, ValidationSucceeds) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::FIRST_NAME,
                   "#first_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::LAST_NAME,
                   "#last_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::EMAIL,
                   "#email");

  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Validation succeeds.
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(true, "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED,
            ProcessAction(action_proto));
}

TEST_F(AutofillActionTest, FallbackFails) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::FIRST_NAME,
                   "#first_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::LAST_NAME,
                   "#last_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::EMAIL,
                   "#email");

  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Validation fails when getting FIRST_NAME.
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Eq(Selector({"#email"})), _))
      .WillOnce(RunOnceCallback<1>(true, "not empty"));
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Eq(Selector({"#first_name"})), _))
      .WillOnce(RunOnceCallback<1>(true, ""));
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Eq(Selector({"#last_name"})), _))
      .WillOnce(RunOnceCallback<1>(true, "not empty"));

  // Fallback fails.
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(Eq(Selector({"#first_name"})), kFirstName, _))
      .WillOnce(RunOnceCallback<2>(ClientStatus(OTHER_ACTION_STATUS)));

  EXPECT_EQ(ProcessedActionStatusProto::MANUAL_FALLBACK,
            ProcessAction(action_proto));
}

TEST_F(AutofillActionTest, FallbackSucceeds) {
  InSequence seq;

  ActionProto action_proto = CreateUseAddressAction();
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::FIRST_NAME,
                   "#first_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::LAST_NAME,
                   "#last_name");
  AddRequiredField(&action_proto, UseAddressProto::RequiredField::EMAIL,
                   "#email");

  // Autofill succeeds.
  EXPECT_CALL(mock_action_delegate_,
              OnFillAddressForm(
                  NotNull(), Eq(Selector({kFakeSelector}).MustBeVisible()), _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  {
    InSequence seq;

    // Validation fails when getting FIRST_NAME.
    EXPECT_CALL(mock_web_controller_,
                OnGetFieldValue(Eq(Selector({"#email"})), _))
        .WillOnce(RunOnceCallback<1>(true, "not empty"));
    EXPECT_CALL(mock_web_controller_,
                OnGetFieldValue(Eq(Selector({"#first_name"})), _))
        .WillOnce(RunOnceCallback<1>(true, ""));
    EXPECT_CALL(mock_web_controller_,
                OnGetFieldValue(Eq(Selector({"#last_name"})), _))
        .WillOnce(RunOnceCallback<1>(true, "not empty"));

    // Fallback succeeds.
    EXPECT_CALL(mock_action_delegate_,
                OnSetFieldValue(Eq(Selector({"#first_name"})), kFirstName, _))
        .WillOnce(RunOnceCallback<2>(OkClientStatus()));

    // Second validation succeeds.
    EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
        .WillRepeatedly(RunOnceCallback<1>(true, "not empty"));
  }
  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED,
            ProcessAction(action_proto));
}

TEST_F(AutofillActionTest, FillCreditCardNoCardSelected) {
  ActionProto action = CreateUseCreditCardAction();
  EXPECT_EQ(ProcessedActionStatusProto::PRECONDITION_FAILED,
            ProcessAction(action));
}

TEST_F(AutofillActionTest, FillCreditCard) {
  ActionProto action = CreateUseCreditCardAction();

  autofill::CreditCard credit_card;
  client_memory_.set_selected_card(
      std::make_unique<autofill::CreditCard>(credit_card));
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(AutofillActionTest, FillCreditCardRequiredFieldsFilled) {
  // Validation succeeds.
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(true, "not empty"));

  ActionProto action = CreateUseCreditCardAction();
  AddRequiredField(
      &action, UseCreditCardProto::RequiredField::CREDIT_CARD_VERIFICATION_CODE,
      "#cvc");
  AddRequiredField(&action,
                   UseCreditCardProto::RequiredField::CREDIT_CARD_EXP_MONTH,
                   "#expmonth");

  autofill::CreditCard credit_card;
  client_memory_.set_selected_card(
      std::make_unique<autofill::CreditCard>(credit_card));
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(AutofillActionTest, FillCreditCardWithFallback) {
  ActionProto action = CreateUseCreditCardAction();
  AddRequiredField(
      &action, UseCreditCardProto::RequiredField::CREDIT_CARD_VERIFICATION_CODE,
      "#cvc");
  AddRequiredField(&action,
                   UseCreditCardProto::RequiredField::CREDIT_CARD_EXP_MONTH,
                   "#expmonth");
  AddRequiredField(
      &action, UseCreditCardProto::RequiredField::CREDIT_CARD_EXP_2_DIGIT_YEAR,
      "#expyear2");
  AddRequiredField(
      &action, UseCreditCardProto::RequiredField::CREDIT_CARD_EXP_4_DIGIT_YEAR,
      "#expyear4");

  // First validation fails.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#cvc"}), _))
      .WillOnce(RunOnceCallback<1>(true, ""));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expmonth"}), _))
      .WillOnce(RunOnceCallback<1>(true, ""));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expyear2"}), _))
      .WillOnce(RunOnceCallback<1>(true, ""));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expyear4"}), _))
      .WillOnce(RunOnceCallback<1>(true, ""));

  // Expect fields to be filled
  Expectation set_cvc =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Selector({"#cvc"}), "123", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Expectation set_expmonth =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Selector({"#expmonth"}), "09", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Expectation set_expyear2 =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Selector({"#expyear2"}), "24", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Expectation set_expyear4 =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Selector({"#expyear4"}), "2024", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // After fallback, second validation succeeds.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#cvc"}), _))
      .After(set_cvc)
      .WillOnce(RunOnceCallback<1>(true, "not empty"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expmonth"}), _))
      .After(set_expmonth)
      .WillOnce(RunOnceCallback<1>(true, "not empty"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expyear2"}), _))
      .After(set_expyear2)
      .WillOnce(RunOnceCallback<1>(true, "not empty"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expyear4"}), _))
      .After(set_expyear4)
      .WillOnce(RunOnceCallback<1>(true, "not empty"));

  autofill::CreditCard credit_card;
  credit_card.SetExpirationMonth(9);
  credit_card.SetExpirationYear(2024);
  client_memory_.set_selected_card(
      std::make_unique<autofill::CreditCard>(credit_card));
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(AutofillActionTest, ForcedFallback) {
  ActionProto action = CreateUseCreditCardAction();
  auto* cvc_required = AddRequiredField(
      &action, UseCreditCardProto::RequiredField::CREDIT_CARD_VERIFICATION_CODE,
      "#cvc");
  cvc_required->set_forced(true);
  cvc_required->set_simulate_key_presses(true);
  cvc_required->set_delay_in_millisecond(1000);

  // No field is ever empty
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(true, "not empty"));

  // But we still want the CVC filled, with simulated keypresses.
  Expectation set_cvc =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Selector({"#cvc"}), "123", true, 1000, _))
          .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  autofill::CreditCard credit_card;
  client_memory_.set_selected_card(
      std::make_unique<autofill::CreditCard>(credit_card));
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

}  // namespace
}  // namespace autofill_assistant
