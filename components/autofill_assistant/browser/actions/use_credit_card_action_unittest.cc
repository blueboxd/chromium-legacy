// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/use_credit_card_action.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "components/autofill_assistant/browser/web/web_controller_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Expectation;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::SaveArgPointee;

class UseCreditCardActionTest : public testing::Test {
 public:
  void SetUp() override {
    // Build two identical autofill profiles. One for the memory, one for the
    // mock.
    auto autofill_profile = std::make_unique<autofill::AutofillProfile>(
        base::GenerateGUID(), autofill::test::kEmptyOrigin);
    autofill::test::SetProfileInfo(autofill_profile.get(), kFirstName, "",
                                   kLastName, kEmail, "", "", "", "", "", "",
                                   "", "");
    user_data_.selected_addresses_[kAddressName] = std::move(autofill_profile);
    ON_CALL(mock_personal_data_manager_, GetProfileByGUID)
        .WillByDefault(Return(&autofill_profile_));
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, RunElementChecks)
        .WillByDefault(Invoke([this](BatchElementChecker* checker) {
          checker->Run(&mock_web_controller_);
        }));
    ON_CALL(mock_action_delegate_, OnShortWaitForElement(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus()));
  }

 protected:
  const char* const kAddressName = "billing";
  const char* const kFakeSelector = "#selector";
  const char* const kSelectionPrompt = "prompt";
  const char* const kFirstName = "FirstName";
  const char* const kLastName = "LastName";
  const char* const kEmail = "foobar@gmail.com";

  ActionProto CreateUseCreditCardAction() {
    ActionProto action;
    action.mutable_use_card()->mutable_form_field_element()->add_selectors(
        kFakeSelector);
    return action;
  }

  UseCreditCardProto::RequiredField* AddRequiredField(
      ActionProto* action,
      std::string value_expression,
      std::string selector) {
    auto* required_field = action->mutable_use_card()->add_required_fields();
    required_field->set_value_expression(value_expression);
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
    UseCreditCardAction action(&mock_action_delegate_, action_proto);
    ProcessedActionProto capture;
    EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&capture));
    action.ProcessAction(callback_.Get());
    return capture.status();
  }

  base::MockCallback<Action::ProcessActionCallback> callback_;
  MockPersonalDataManager mock_personal_data_manager_;
  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  UserData user_data_;

  autofill::AutofillProfile autofill_profile_;
};

TEST_F(UseCreditCardActionTest, FillCreditCardNoCardSelected) {
  ActionProto action = CreateUseCreditCardAction();
  EXPECT_EQ(ProcessedActionStatusProto::PRECONDITION_FAILED,
            ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, FillCreditCard) {
  ActionProto action = CreateUseCreditCardAction();

  autofill::CreditCard credit_card;
  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(credit_card);
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, FillCreditCardRequiredFieldsFilled) {
  // Validation succeeds.
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  ActionProto action = CreateUseCreditCardAction();
  AddRequiredField(
      &action,
      base::NumberToString(static_cast<int>(
          UseCreditCardProto::RequiredField::CREDIT_CARD_VERIFICATION_CODE)),
      "#cvc");
  AddRequiredField(&action,
                   base::NumberToString(static_cast<int>(
                       autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH)),
                   "#expmonth");

  autofill::CreditCard credit_card;
  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(credit_card);
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, FillCreditCardWithFallback) {
  ON_CALL(mock_action_delegate_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action = CreateUseCreditCardAction();
  AddRequiredField(
      &action,
      base::NumberToString(static_cast<int>(
          UseCreditCardProto::RequiredField::CREDIT_CARD_VERIFICATION_CODE)),
      "#cvc");
  AddRequiredField(&action,
                   base::NumberToString(static_cast<int>(
                       autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH)),
                   "#expmonth");
  AddRequiredField(
      &action,
      base::NumberToString(static_cast<int>(
          autofill::ServerFieldType::CREDIT_CARD_EXP_2_DIGIT_YEAR)),
      "#expyear2");
  AddRequiredField(
      &action,
      base::NumberToString(static_cast<int>(
          autofill::ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR)),
      "#expyear4");
  AddRequiredField(&action,
                   base::NumberToString(static_cast<int>(
                       autofill::ServerFieldType::CREDIT_CARD_NAME_FULL)),
                   "#card_name");
  AddRequiredField(&action,
                   base::NumberToString(static_cast<int>(
                       autofill::ServerFieldType::CREDIT_CARD_NUMBER)),
                   "#card_number");
  AddRequiredField(&action,
                   base::NumberToString(static_cast<int>(
                       UseCreditCardProto::RequiredField::CREDIT_CARD_NETWORK)),
                   "#network");

  // First validation fails.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#cvc"}), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expmonth"}), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expyear2"}), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expyear4"}), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Selector({"#card_name"}), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Selector({"#card_number"}), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#network"}), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));

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
  Expectation set_cardholder_name =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Selector({"#card_name"}), "Jon Doe", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Expectation set_card_number =
      EXPECT_CALL(
          mock_action_delegate_,
          OnSetFieldValue(Selector({"#card_number"}), "4111111111111111", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  Expectation set_card_network =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Selector({"#network"}), "visa", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // After fallback, second validation succeeds.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#cvc"}), _))
      .After(set_cvc)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expmonth"}), _))
      .After(set_expmonth)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expyear2"}), _))
      .After(set_expyear2)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#expyear4"}), _))
      .After(set_expyear4)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Selector({"#card_name"}), _))
      .After(set_expyear4)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Selector({"#card_number"}), _))
      .After(set_expyear4)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#network"}), _))
      .After(set_card_network)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  autofill::CreditCard credit_card;
  credit_card.SetExpirationMonth(9);
  credit_card.SetExpirationYear(2024);
  credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                         base::UTF8ToUTF16("Jon Doe"));
  credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                         base::UTF8ToUTF16("4111111111111111"));
  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(credit_card);
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, ForcedFallbackWithKeystrokes) {
  ON_CALL(mock_action_delegate_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action = CreateUseCreditCardAction();
  auto* cvc_required = AddRequiredField(
      &action,
      base::NumberToString(static_cast<int>(
          UseCreditCardProto::RequiredField::CREDIT_CARD_VERIFICATION_CODE)),
      "#cvc");
  cvc_required->set_forced(true);
  cvc_required->set_fill_strategy(SIMULATE_KEY_PRESSES);
  cvc_required->set_delay_in_millisecond(1000);

  // No field is ever empty
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  // But we still want the CVC filled, with simulated keypresses.
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(Selector({"#cvc"}), "123", true, 1000, _))
      .WillOnce(RunOnceCallback<4>(OkClientStatus()));

  autofill::CreditCard credit_card;
  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(credit_card);
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED, ProcessAction(action));
}

TEST_F(UseCreditCardActionTest, AutofillFailureWithoutRequiredFieldsIsFatal) {
  ActionProto action_proto = CreateUseCreditCardAction();

  autofill::CreditCard credit_card;
  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(credit_card);
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(ClientStatus(OTHER_ACTION_STATUS)));

  ProcessedActionProto processed_action;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_action));

  UseCreditCardAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(processed_action.status(),
            ProcessedActionStatusProto::OTHER_ACTION_STATUS);
  EXPECT_EQ(processed_action.has_status_details(), false);
}

TEST_F(UseCreditCardActionTest,
       AutofillFailureWithRequiredFieldsLaunchesFallback) {
  ON_CALL(mock_action_delegate_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto = CreateUseCreditCardAction();
  AddRequiredField(
      &action_proto,
      base::NumberToString(static_cast<int>(
          UseCreditCardProto::RequiredField::CREDIT_CARD_VERIFICATION_CODE)),
      "#cvc");

  autofill::CreditCard credit_card;
  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(credit_card);
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(
          FillAutofillErrorStatus(ClientStatus(OTHER_ACTION_STATUS))));

  // First validation fails.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#cvc"}), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  // Fill CVC.
  Expectation set_cvc =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Selector({"#cvc"}), "123", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(Selector({"#cvc"}), _))
      .After(set_cvc)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  ProcessedActionProto processed_action;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&processed_action));

  UseCreditCardAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(processed_action.status(),
            ProcessedActionStatusProto::ACTION_APPLIED);
  EXPECT_EQ(processed_action.status_details()
                .autofill_error_info()
                .autofill_error_status(),
            OTHER_ACTION_STATUS);
}

TEST_F(UseCreditCardActionTest, FallbackForCardExpirationSucceeds) {
  ON_CALL(mock_action_delegate_, GetElementTag(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));

  ActionProto action_proto = CreateUseCreditCardAction();
  AddRequiredField(&action_proto, "${53} - ${55}", "#expiration_date");

  // Autofill succeeds.
  autofill::CreditCard credit_card;
  credit_card.SetExpirationMonth(9);
  credit_card.SetExpirationYear(2050);
  credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                         base::UTF8ToUTF16("Jon Doe"));
  credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                         base::UTF8ToUTF16("4111111111111111"));
  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(credit_card);
  EXPECT_CALL(mock_action_delegate_, OnGetFullCard(_))
      .WillOnce(RunOnceCallback<0>(credit_card, base::UTF8ToUTF16("123")));
  EXPECT_CALL(mock_action_delegate_,
              OnFillCardForm(_, base::UTF8ToUTF16("123"),
                             Selector({kFakeSelector}).MustBeVisible(), _))
      .WillOnce(RunOnceCallback<3>(OkClientStatus()));

  // Validation fails when getting expiration date.
  EXPECT_CALL(mock_web_controller_,
              OnGetFieldValue(Eq(Selector({"#expiration_date"})), _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));

  // Fallback succeeds.
  Expectation set_expiration_date =
      EXPECT_CALL(
          mock_action_delegate_,
          OnSetFieldValue(Eq(Selector({"#expiration_date"})), "09 - 2050", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  // Second validation succeeds.
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .After(set_expiration_date)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  EXPECT_EQ(ProcessedActionStatusProto::ACTION_APPLIED,
            ProcessAction(action_proto));
}

}  // namespace
}  // namespace autofill_assistant
