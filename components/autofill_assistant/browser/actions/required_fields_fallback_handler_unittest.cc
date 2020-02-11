// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/required_fields_fallback_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Expectation;
using ::testing::Invoke;

RequiredFieldsFallbackHandler::RequiredField CreateRequiredField(
    const std::string& value_expression,
    const std::vector<std::string>& selector) {
  RequiredFieldsFallbackHandler::RequiredField required_field;
  required_field.value_expression = value_expression;
  required_field.selector = Selector(selector);
  required_field.status = RequiredFieldsFallbackHandler::EMPTY;
  return required_field;
}

class RequiredFieldsFallbackHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    ON_CALL(mock_action_delegate_, RunElementChecks)
        .WillByDefault(Invoke([this](BatchElementChecker* checker) {
          checker->Run(&mock_web_controller_);
        }));
    ON_CALL(mock_action_delegate_, GetElementTag(_, _))
        .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "INPUT"));
    ON_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _))
        .WillByDefault(RunOnceCallback<2>(OkClientStatus()));
  }

 protected:
  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
};

TEST_F(RequiredFieldsFallbackHandlerTest,
       AutofillFailureExitsEarlyForEmptyRequiredFields) {
  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), OTHER_ACTION_STATUS);
            EXPECT_FALSE(detail_status.has_value());
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      ClientStatus(OTHER_ACTION_STATUS), std::move(fallback_data),
      std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       AddsMissingOrEmptyFallbackFieldToError) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  fields.emplace_back(CreateRequiredField("51", {"#card_name"}));
  fields.emplace_back(CreateRequiredField("52", {"#card_number"}));
  fields.emplace_back(CreateRequiredField("-3", {"#card_network"}));

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();
  fallback_data->field_values.emplace(
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL),
      "John Doe");
  fallback_data->field_values.emplace(
      static_cast<int>(UseCreditCardProto::RequiredField::CREDIT_CARD_NETWORK),
      "");

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), MANUAL_FALLBACK);
            ASSERT_TRUE(detail_status.has_value());
            ASSERT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error_size(),
                      2);
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(0)
                          .value_expression(),
                      "52");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(0)
                            .no_fallback_value());
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(1)
                          .value_expression(),
                      "-3");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(1)
                            .no_fallback_value());
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), std::move(fallback_data), std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, AddsFirstFieldFillingError) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));
  ON_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _))
      .WillByDefault(RunOnceCallback<2>(ClientStatus(OTHER_ACTION_STATUS)));

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  fields.emplace_back(CreateRequiredField("51", {"#card_name"}));
  fields.emplace_back(CreateRequiredField("52", {"#card_number"}));

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();
  fallback_data->field_values.emplace(
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL),
      "John Doe");
  fallback_data->field_values.emplace(
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NUMBER),
      "4111111111111111");

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), MANUAL_FALLBACK);
            ASSERT_TRUE(detail_status.has_value());
            ASSERT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error_size(),
                      1);
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(0)
                          .value_expression(),
                      "51");
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(0)
                          .status(),
                      OTHER_ACTION_STATUS);
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), std::move(fallback_data), std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, DoesNotFallbackIfFieldsAreFilled) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _)).Times(0);

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  fields.emplace_back(CreateRequiredField("51", {"#card_name"}));

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), std::move(fallback_data), std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FillsEmptyRequiredField) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  Expectation set_value =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Eq(Selector({"#card_name"})), "John Doe", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .After(set_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "John Doe"));

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  fields.emplace_back(CreateRequiredField("51", {"#card_name"}));

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();
  fallback_data->field_values.emplace(
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL),
      "John Doe");

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), std::move(fallback_data), std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FallsBackForForcedFilledField) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_action_delegate_,
              OnSetFieldValue(Eq(Selector({"#card_name"})), "John Doe", _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  auto field = CreateRequiredField("51", {"#card_name"});
  field.forced = true;
  fields.emplace_back(field);

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();
  fallback_data->field_values.emplace(
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_NAME_FULL),
      "John Doe");

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), std::move(fallback_data), std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FailsIfForcedFieldDidNotGetFilled) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), "value"));
  EXPECT_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _)).Times(0);

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  auto field = CreateRequiredField("51", {"#card_name"});
  field.forced = true;
  fields.emplace_back(field);

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), MANUAL_FALLBACK);
            ASSERT_TRUE(detail_status.has_value());
            ASSERT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error_size(),
                      1);
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(0)
                          .value_expression(),
                      "51");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(0)
                            .no_fallback_value());
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), std::move(fallback_data), std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, FillsFieldWithPattern) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  Expectation set_value =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Eq(Selector({"#card_expiry"})), "08/2050", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .After(set_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  fields.emplace_back(CreateRequiredField("${53}/${55}", {"#card_expiry"}));

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();
  fallback_data->field_values.emplace(
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_MONTH), "08");
  fallback_data->field_values.emplace(
      static_cast<int>(autofill::ServerFieldType::CREDIT_CARD_EXP_4_DIGIT_YEAR),
      "2050");

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), std::move(fallback_data), std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest,
       FailsToFillFieldWithUnknownOrEmptyKey) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .Times(2)
      .WillRepeatedly(RunOnceCallback<1>(OkClientStatus(), ""));
  EXPECT_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _)).Times(0);

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  fields.emplace_back(CreateRequiredField("53", {"#card_expiry"}));
  fields.emplace_back(CreateRequiredField("-3", {"#card_network"}));

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();
  fallback_data->field_values.emplace(
      static_cast<int>(UseCreditCardProto::RequiredField::CREDIT_CARD_NETWORK),
      "");

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), MANUAL_FALLBACK);
            ASSERT_TRUE(detail_status.has_value());
            ASSERT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error_size(),
                      2);
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(0)
                          .value_expression(),
                      "53");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(0)
                            .no_fallback_value());
            EXPECT_EQ(detail_status.value()
                          .details()
                          .autofill_error_info()
                          .autofill_field_error(1)
                          .value_expression(),
                      "-3");
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(1)
                            .no_fallback_value());
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), std::move(fallback_data), std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, IgnoresNonIntegerKeys) {
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), ""));
  Expectation set_value =
      EXPECT_CALL(mock_action_delegate_,
                  OnSetFieldValue(Eq(Selector({"#card_expiry"})), "${KEY}", _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .After(set_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "not empty"));

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  fields.emplace_back(CreateRequiredField("${KEY}", {"#card_expiry"}));

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback =
          base::BindOnce([](const ClientStatus& status,
                            const base::Optional<ClientStatus>& detail_status) {
            EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
          });

  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), std::move(fallback_data), std::move(callback));
}

TEST_F(RequiredFieldsFallbackHandlerTest, AddsAllProfileFields) {
  std::unordered_map<int, std::string> expected_values = {
      {3, "Alpha"},
      {4, "Beta"},
      {5, "Gamma"},
      {6, "B"},
      {7, "Alpha Beta Gamma"},
      {9, "alpha@google.com"},
      {10, "1234567"},
      {11, "79"},
      {12, "41"},
      {13, "0791234567"},
      {14, "+41791234567"},
      {30, "Brandschenkestrasse 110"},
      {31, "Google Building 110"},
      {33, "Zurich"},
      {34, "Canton Zurich"},
      {35, "8002"},
      {36, "Switzerland"},
      {60, "Google"},
      {77, "Brandschenkestrasse 110\nGoogle Building 110"}};

  auto profile = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), autofill::test::kEmptyOrigin);
  autofill::test::SetProfileInfo(
      profile.get(), "Alpha", "Beta", "Gamma", "alpha@google.com", "Google",
      "Brandschenkestrasse 110", "Google Building 110", "Zurich",
      "Canton Zurich", "8002", "CH", "+41791234567");
  RequiredFieldsFallbackHandler::FallbackData fallback_data;
  fallback_data.AddFormGroup(*profile);

  for (auto iter = expected_values.begin(); iter != expected_values.end();
       ++iter) {
    ASSERT_TRUE(fallback_data.GetValue(iter->first).has_value());
    EXPECT_EQ(fallback_data.GetValue(iter->first).value(), iter->second);
  }
}

TEST_F(RequiredFieldsFallbackHandlerTest, AddsAllCreditCardFields) {
  std::unordered_map<int, std::string> expected_values = {
      {51, "Alpha Beta Gamma"},
      {52, "4111111111111111"},
      {53, "08"},
      {54, "50"},
      {55, "2050"},
      {56, "08/50"},
      {57, "08/2050"},
      {58, "Visa"},
      {91, "Alpha"},
      {92, "Gamma"}};

  auto card = std::make_unique<autofill::CreditCard>(
      base::GenerateGUID(), autofill::test::kEmptyOrigin);
  autofill::test::SetCreditCardInfo(card.get(), "Alpha Beta Gamma",
                                    "4111111111111111", "8", "2050", "");
  RequiredFieldsFallbackHandler::FallbackData fallback_data;
  fallback_data.AddFormGroup(*card);

  for (auto iter = expected_values.begin(); iter != expected_values.end();
       ++iter) {
    ASSERT_TRUE(fallback_data.GetValue(iter->first).has_value());
    EXPECT_EQ(fallback_data.GetValue(iter->first).value(), iter->second);
  }
}

}  // namespace
}  // namespace autofill_assistant
