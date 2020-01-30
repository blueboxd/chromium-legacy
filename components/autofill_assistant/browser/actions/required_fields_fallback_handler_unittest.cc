// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/required_fields_fallback_handler.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Expectation;
using ::testing::Invoke;

RequiredFieldsFallbackHandler::RequiredField CreateRequiredField(
    int key,
    const std::vector<std::string>& selector) {
  RequiredFieldsFallbackHandler::RequiredField required_field;
  required_field.fallback_key = key;
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

TEST_F(RequiredFieldsFallbackHandlerTest, AddsMissingFallbackFieldToError) {
  ON_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .WillByDefault(RunOnceCallback<1>(OkClientStatus(), ""));

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  fields.emplace_back(CreateRequiredField(
      UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME,
      std::vector<std::string>{"card_name"}));
  fields.emplace_back(
      CreateRequiredField(UseCreditCardProto::RequiredField::CREDIT_CARD_NUMBER,
                          std::vector<std::string>{"card_number"}));

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();
  fallback_data->field_values.emplace(
      static_cast<int>(
          UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME),
      "John Doe");

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
                          .field_key(),
                      UseCreditCardProto::RequiredField::CREDIT_CARD_NUMBER);
            EXPECT_TRUE(detail_status.value()
                            .details()
                            .autofill_error_info()
                            .autofill_field_error(0)
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
  fields.emplace_back(CreateRequiredField(
      UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME,
      std::vector<std::string>{"card_name"}));
  fields.emplace_back(
      CreateRequiredField(UseCreditCardProto::RequiredField::CREDIT_CARD_NUMBER,
                          std::vector<std::string>{"card_number"}));

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();
  fallback_data->field_values.emplace(
      static_cast<int>(
          UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME),
      "John Doe");
  fallback_data->field_values.emplace(
      static_cast<int>(UseCreditCardProto::RequiredField::CREDIT_CARD_NUMBER),
      "4111111111111111");

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback = base::BindOnce([](const ClientStatus& status,
                                   const base::Optional<ClientStatus>&
                                       detail_status) {
        EXPECT_EQ(status.proto_status(), MANUAL_FALLBACK);
        ASSERT_TRUE(detail_status.has_value());
        ASSERT_EQ(detail_status.value()
                      .details()
                      .autofill_error_info()
                      .autofill_field_error_size(),
                  1);
        EXPECT_EQ(
            detail_status.value()
                .details()
                .autofill_error_info()
                .autofill_field_error(0)
                .field_key(),
            UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME);
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
  fields.emplace_back(CreateRequiredField(
      UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME,
      std::vector<std::string>{"card_name"}));

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
      EXPECT_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _))
          .WillOnce(RunOnceCallback<2>(OkClientStatus()));
  EXPECT_CALL(mock_web_controller_, OnGetFieldValue(_, _))
      .After(set_value)
      .WillOnce(RunOnceCallback<1>(OkClientStatus(), "John Doe"));

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  fields.emplace_back(CreateRequiredField(
      UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME,
      std::vector<std::string>{"card_name"}));

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();
  fallback_data->field_values.emplace(
      static_cast<int>(
          UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME),
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
  EXPECT_CALL(mock_action_delegate_, OnSetFieldValue(_, _, _))
      .WillOnce(RunOnceCallback<2>(OkClientStatus()));

  std::vector<RequiredFieldsFallbackHandler::RequiredField> fields;
  auto field = CreateRequiredField(
      UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME,
      std::vector<std::string>{"card_name"});
  field.forced = true;
  fields.emplace_back(field);

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();
  fallback_data->field_values.emplace(
      static_cast<int>(
          UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME),
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
  auto field = CreateRequiredField(
      UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME,
      std::vector<std::string>{"card_name"});
  field.forced = true;
  fields.emplace_back(field);

  auto fallback_data =
      std::make_unique<RequiredFieldsFallbackHandler::FallbackData>();

  RequiredFieldsFallbackHandler fallback_handler(fields,
                                                 &mock_action_delegate_);

  base::OnceCallback<void(const ClientStatus&,
                          const base::Optional<ClientStatus>&)>
      callback = base::BindOnce([](const ClientStatus& status,
                                   const base::Optional<ClientStatus>&
                                       detail_status) {
        EXPECT_EQ(status.proto_status(), MANUAL_FALLBACK);
        ASSERT_TRUE(detail_status.has_value());
        ASSERT_EQ(detail_status.value()
                      .details()
                      .autofill_error_info()
                      .autofill_field_error_size(),
                  1);
        EXPECT_EQ(
            detail_status.value()
                .details()
                .autofill_error_info()
                .autofill_field_error(0)
                .field_key(),
            UseCreditCardProto::RequiredField::CREDIT_CARD_CARD_HOLDER_NAME);
        EXPECT_TRUE(detail_status.value()
                        .details()
                        .autofill_error_info()
                        .autofill_field_error(0)
                        .no_fallback_value());
      });

  fallback_handler.CheckAndFallbackRequiredFields(
      OkClientStatus(), std::move(fallback_data), std::move(callback));
}

}  // namespace
}  // namespace autofill_assistant
