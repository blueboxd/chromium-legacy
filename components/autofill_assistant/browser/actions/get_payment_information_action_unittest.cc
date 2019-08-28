// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/get_payment_information_action.h"

#include <utility>

#include "base/guid.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/mock_personal_data_manager.h"
#include "components/autofill_assistant/browser/mock_website_login_fetcher.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {
const char kFakeUrl[] = "https://www.example.com";
const char kFakeUsername[] = "user@example.com";
const char kFakePassword[] = "example_password";

const char kMemoryLocation[] = "billing";
const char kName[] = "John Doe";
const char kEmail[] = "john@doe.com";
const char kPhone[] = "+1 234-567-8901";
}  // namespace

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;

class GetPaymentInformationActionTest
    : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ON_CALL(mock_action_delegate_, GetClientMemory)
        .WillByDefault(Return(&client_memory_));
    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginFetcher)
        .WillByDefault(Return(&mock_website_login_fetcher_));
    ON_CALL(mock_action_delegate_, GetPaymentInformation(_, _))
        .WillByDefault(
            Invoke([](std::unique_ptr<PaymentRequestOptions> options,
                      std::unique_ptr<PaymentInformation> information) {
              std::move(options->confirm_callback).Run(std::move(information));
            }));

    ON_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(_, _))
        .WillByDefault(
            RunOnceCallback<1>(std::vector<WebsiteLoginFetcher::Login>{
                WebsiteLoginFetcher::Login(GURL(kFakeUrl), kFakeUsername)}));
    ON_CALL(mock_website_login_fetcher_, OnGetPasswordForLogin(_, _))
        .WillByDefault(RunOnceCallback<1>(true, kFakePassword));

    content::WebContentsTester::For(web_contents())
        ->SetLastCommittedURL(GURL(kFakeUrl));
    ON_CALL(mock_action_delegate_, GetWebContents())
        .WillByDefault(Return(web_contents()));
  }

 protected:
  base::MockCallback<Action::ProcessActionCallback> callback_;
  MockPersonalDataManager mock_personal_data_manager_;
  MockWebsiteLoginFetcher mock_website_login_fetcher_;
  MockActionDelegate mock_action_delegate_;
  ClientMemory client_memory_;
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

TEST_F(GetPaymentInformationActionTest, SelectLogin) {
  ActionProto action_proto;
  auto* login_details =
      action_proto.mutable_get_payment_information()->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("payload");

  // Action should fetch the logins, but not the passwords.
  EXPECT_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(GURL(kFakeUrl), _))
      .Times(1);
  EXPECT_CALL(mock_website_login_fetcher_, OnGetPasswordForLogin(_, _))
      .Times(0);

  ON_CALL(mock_action_delegate_, GetPaymentInformation(_, _))
      .WillByDefault(
          Invoke([](std::unique_ptr<PaymentRequestOptions> options,
                    std::unique_ptr<PaymentInformation> information) {
            information->succeed = true;
            information->login_choice_identifier.assign(
                options->login_choices[0].identifier);
            std::move(options->confirm_callback).Run(std::move(information));
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::payment_details,
                   Property(&PaymentDetails::login_payload, "payload"))))));
  GetPaymentInformationAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(GetPaymentInformationActionTest, LoginChoiceAutomaticIfNoOtherOptions) {
  ActionProto action_proto;
  auto* payment_information = action_proto.mutable_get_payment_information();
  payment_information->set_request_terms_and_conditions(false);
  auto* login_details = payment_information->mutable_login_details();
  auto* guest_login_option = login_details->add_login_options();
  guest_login_option->mutable_custom()->set_label("Guest Checkout");
  guest_login_option->set_payload("guest");
  guest_login_option->set_choose_automatically_if_no_other_options(true);
  auto* password_login_option = login_details->add_login_options();
  password_login_option->mutable_password_manager();
  password_login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginFetcher::Login>{}));

  EXPECT_CALL(mock_action_delegate_, GetPaymentInformation(_, _)).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(
          AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                Property(&ProcessedActionProto::payment_details,
                         Property(&PaymentDetails::login_payload, "guest"))))));
  GetPaymentInformationAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(GetPaymentInformationActionTest, SelectLoginFailsIfNoOptionAvailable) {
  ActionProto action_proto;
  auto* payment_information = action_proto.mutable_get_payment_information();
  auto* login_details = payment_information->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginFetcher::Login>{}));

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              PAYMENT_REQUEST_ERROR))));
  GetPaymentInformationAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(GetPaymentInformationActionTest, SelectContactDetails) {
  ActionProto action_proto;
  auto* payment_information_proto =
      action_proto.mutable_get_payment_information();
  payment_information_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      payment_information_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);

  ON_CALL(mock_action_delegate_, GetPaymentInformation(_, _))
      .WillByDefault(
          Invoke([](std::unique_ptr<PaymentRequestOptions> options,
                    std::unique_ptr<PaymentInformation> information) {
            information->succeed = true;
            information->payer_name.assign(kName);
            information->payer_phone.assign(kPhone);
            information->payer_email.assign(kEmail);
            std::move(options->confirm_callback).Run(std::move(information));
          }));

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::payment_details,
                           Property(&PaymentDetails::payer_email, kEmail))))));
  GetPaymentInformationAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(client_memory_.has_selected_address(kMemoryLocation), true);
  auto* profile = client_memory_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL), base::UTF8ToUTF16(kName));
  EXPECT_EQ(profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER),
            base::UTF8ToUTF16(kPhone));
  EXPECT_EQ(profile->GetRawInfo(autofill::EMAIL_ADDRESS),
            base::UTF8ToUTF16(kEmail));
}

TEST_F(GetPaymentInformationActionTest, SelectPaymentMethod) {
  ActionProto action_proto;
  action_proto.mutable_get_payment_information()->set_ask_for_payment(true);
  action_proto.mutable_get_payment_information()
      ->set_request_terms_and_conditions(false);

  autofill::AutofillProfile billing_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&billing_profile, "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");
  ON_CALL(mock_personal_data_manager_, GetProfileByGUID(billing_profile.guid()))
      .WillByDefault(Return(&billing_profile));

  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2020",
                                    billing_profile.guid());

  ON_CALL(mock_action_delegate_, GetPaymentInformation(_, _))
      .WillByDefault(
          Invoke([=](std::unique_ptr<PaymentRequestOptions> options,
                     std::unique_ptr<PaymentInformation> information) {
            information->card =
                std::make_unique<autofill::CreditCard>(credit_card);
            information->succeed = true;
            std::move(options->confirm_callback).Run(std::move(information));
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::payment_details,
                   Property(&PaymentDetails::card_issuer_network, "visa"))))));
  GetPaymentInformationAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(client_memory_.has_selected_card(), true);
  EXPECT_THAT(client_memory_.selected_card()->Compare(credit_card), Eq(0));
}

TEST_F(GetPaymentInformationActionTest,
       MandatoryPostalCodeWithoutErrorMessageFails) {
  ActionProto action_proto;
  action_proto.mutable_get_payment_information()->set_ask_for_payment(true);
  action_proto.mutable_get_payment_information()
      ->set_require_billing_postal_code(true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  GetPaymentInformationAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
