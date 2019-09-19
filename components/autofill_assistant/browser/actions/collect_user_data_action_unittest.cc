// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/collect_user_data_action.h"

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
}  // namespace

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;

class CollectUserDataActionTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ON_CALL(mock_action_delegate_, GetClientMemory)
        .WillByDefault(Return(&client_memory_));
    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginFetcher)
        .WillByDefault(Return(&mock_website_login_fetcher_));
    ON_CALL(mock_action_delegate_, CollectUserData(_, _))
        .WillByDefault(Invoke([](std::unique_ptr<CollectUserDataOptions>
                                     collect_user_data_options,
                                 std::unique_ptr<UserData> user_data) {
          std::move(collect_user_data_options->confirm_callback)
              .Run(std::move(user_data));
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

TEST_F(CollectUserDataActionTest, PromptIsShown) {
  const char kPrompt[] = "Some message.";

  ActionProto action_proto;
  action_proto.mutable_collect_user_data()->set_prompt(kPrompt);
  CollectUserDataAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(kPrompt));
  EXPECT_CALL(callback_, Run(_));
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectLogin) {
  ActionProto action_proto;
  auto* login_details =
      action_proto.mutable_collect_user_data()->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("payload");

  // Action should fetch the logins, but not the passwords.
  EXPECT_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(GURL(kFakeUrl), _))
      .Times(1);
  EXPECT_CALL(mock_website_login_fetcher_, OnGetPasswordForLogin(_, _))
      .Times(0);

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
             std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->login_choice_identifier.assign(
                collect_user_data_options->login_choices[0].identifier);
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
          }));

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::login_payload,
                                    "payload"))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, LoginChoiceAutomaticIfNoOtherOptions) {
  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_custom()->set_label("Guest Checkout");
  login_option->set_payload("guest");
  login_option->set_choose_automatically_if_no_other_options(true);
  login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginFetcher::Login>{}));

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_, _)).Times(0);
  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::login_payload,
                                    "guest"))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectLoginFailsIfNoOptionAvailable) {
  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  auto* login_details = collect_user_data->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("password_manager");

  ON_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(_, _))
      .WillByDefault(
          RunOnceCallback<1>(std::vector<WebsiteLoginFetcher::Login>{}));

  EXPECT_CALL(callback_, Run(Pointee(Property(&ProcessedActionProto::status,
                                              COLLECT_USER_DATA_ERROR))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectContactDetails) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);
  contact_details_proto->set_request_payer_phone(true);

  autofill::AutofillProfile contact_profile;
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                             base::UTF8ToUTF16("Marion Mitchell Morrison"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_FIRST,
                             base::UTF8ToUTF16("Marion"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_MIDDLE,
                             base::UTF8ToUTF16("Mitchell"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::NAME_LAST,
                             base::UTF8ToUTF16("Morrison"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::EMAIL_ADDRESS,
                             base::UTF8ToUTF16("marion@me.xyz"));
  contact_profile.SetRawInfo(autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
                             base::UTF8ToUTF16("16505678910"));

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [=](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
              std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->contact_profile =
                std::make_unique<autofill::AutofillProfile>(contact_profile);
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
          }));

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::payer_email,
                                    "marion@me.xyz"))))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(client_memory_.has_selected_address(kMemoryLocation), true);
  auto* profile = client_memory_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL),
            base::UTF8ToUTF16("Marion Mitchell Morrison"));
  EXPECT_EQ(profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER),
            base::UTF8ToUTF16("16505678910"));
  EXPECT_EQ(profile->GetRawInfo(autofill::EMAIL_ADDRESS),
            base::UTF8ToUTF16("marion@me.xyz"));
}

TEST_F(CollectUserDataActionTest, SelectPaymentMethod) {
  ActionProto action_proto;
  action_proto.mutable_collect_user_data()->set_request_payment_method(true);
  action_proto.mutable_collect_user_data()->set_request_terms_and_conditions(
      false);

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

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [=](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
              std::unique_ptr<UserData> user_data) {
            user_data->card =
                std::make_unique<autofill::CreditCard>(credit_card);
            user_data->succeed = true;
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::collect_user_data_result,
                   Property(&CollectUserDataResultProto::card_issuer_network,
                            "visa"))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(client_memory_.has_selected_card(), true);
  EXPECT_THAT(client_memory_.selected_card()->Compare(credit_card), Eq(0));
}

TEST_F(CollectUserDataActionTest, MandatoryPostalCodeWithoutErrorMessageFails) {
  ActionProto action_proto;
  action_proto.mutable_collect_user_data()->set_request_payment_method(true);
  action_proto.mutable_collect_user_data()->set_require_billing_postal_code(
      true);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ContactDetailsCanHandleUtf8) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* contact_details_proto =
      collect_user_data_proto->mutable_contact_details();
  contact_details_proto->set_contact_details_name(kMemoryLocation);
  contact_details_proto->set_request_payer_name(true);
  contact_details_proto->set_request_payer_email(true);

  // Name = 艾丽森 in UTF-8.
  autofill::AutofillProfile contact_profile;
  contact_profile.SetRawInfo(
      autofill::ServerFieldType::NAME_FULL,
      base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE"));
  contact_profile.SetRawInfo(
      autofill::ServerFieldType::EMAIL_ADDRESS,
      base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE@example.com"));

  ON_CALL(mock_action_delegate_, CollectUserData(_, _))
      .WillByDefault(Invoke(
          [=](std::unique_ptr<CollectUserDataOptions> collect_user_data_options,
              std::unique_ptr<UserData> user_data) {
            user_data->succeed = true;
            user_data->contact_profile =
                std::make_unique<autofill::AutofillProfile>(contact_profile);
            std::move(collect_user_data_options->confirm_callback)
                .Run(std::move(user_data));
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::payer_email,
                       "\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE@example.com"))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(client_memory_.has_selected_address(kMemoryLocation), true);
  auto* profile = client_memory_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL),
            base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE"));
  EXPECT_EQ(
      profile->GetRawInfo(autofill::EMAIL_ADDRESS),
      base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE@example.com"));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Contact) {
  UserData user_data;
  CollectUserDataOptions options;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  user_data.contact_profile = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), kFakeUrl);
  options.request_payer_email = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  user_data.contact_profile->SetRawInfo(
      autofill::ServerFieldType::EMAIL_ADDRESS,
      base::UTF8ToUTF16("joedoe@example.com"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  options.request_payer_name = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  user_data.contact_profile->SetRawInfo(autofill::ServerFieldType::NAME_FULL,
                                        base::UTF8ToUTF16("Joe Doe"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  options.request_payer_phone = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  user_data.contact_profile->SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
      base::UTF8ToUTF16("+1 23 456 789 01"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Payment) {
  UserData user_data;
  CollectUserDataOptions options;

  options.request_payment_method = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  // Valid credit card, but no billing address.
  user_data.card =
      std::make_unique<autofill::CreditCard>(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(user_data.card.get(), "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2020",
                                    /* billing_address_id = */ "");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  // Valid credit card, but invalid billing address.
  user_data.card->set_billing_address_id("invalid");
  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("invalid"))
      .WillByDefault(Return(nullptr));
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  // Incomplete billing address.
  user_data.billing_address = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(user_data.billing_address.get(), "Marion",
                                 "Mitchell", "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 /* zipcode = */ "", "US", "16505678910");
  ON_CALL(mock_personal_data_manager_,
          GetProfileByGUID(user_data.billing_address->guid()))
      .WillByDefault(Return(user_data.billing_address.get()));
  user_data.card->set_billing_address_id(user_data.billing_address->guid());
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  user_data.billing_address->SetRawInfo(autofill::ADDRESS_HOME_ZIP,
                                        base::UTF8ToUTF16("91601"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  // Zip code is optional in Argentinian address.
  user_data.billing_address->SetRawInfo(autofill::ADDRESS_HOME_ZIP,
                                        base::UTF8ToUTF16(""));
  user_data.billing_address->SetRawInfo(autofill::ADDRESS_HOME_COUNTRY,
                                        base::UTF8ToUTF16("AR"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  options.require_billing_postal_code = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  user_data.billing_address->SetRawInfo(autofill::ADDRESS_HOME_ZIP,
                                        base::UTF8ToUTF16("B1675"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Terms) {
  UserData user_data;
  CollectUserDataOptions options;
  options.accept_terms_and_conditions_text.assign("Accept T&C");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  user_data.terms_and_conditions = REQUIRES_REVIEW;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  user_data.terms_and_conditions = ACCEPTED;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Login) {
  UserData user_data;
  CollectUserDataOptions options;
  options.request_login_choice = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  user_data.login_choice_identifier.assign("1");
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_ShippingAddress) {
  UserData user_data;
  CollectUserDataOptions options;
  options.request_shipping = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  // Incomplete address.
  user_data.shipping_address = std::make_unique<autofill::AutofillProfile>(
      base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(user_data.shipping_address.get(), "Marion",
                                 "Mitchell", "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 /* zipcode = */ "", "US", "16505678910");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));

  user_data.shipping_address->SetRawInfo(autofill::ADDRESS_HOME_ZIP,
                                         base::UTF8ToUTF16("91601"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(
      &mock_personal_data_manager_, user_data, options));
}

}  // namespace
}  // namespace autofill_assistant
