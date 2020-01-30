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
#include "components/autofill_assistant/browser/user_model.h"
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

void SetDateTimeProto(DateTimeProto* proto,
                      int year,
                      int month,
                      int day,
                      int hour,
                      int minute,
                      int second) {
  proto->mutable_date()->set_year(year);
  proto->mutable_date()->set_month(month);
  proto->mutable_date()->set_day(day);
  proto->mutable_time()->set_hour(hour);
  proto->mutable_time()->set_minute(minute);
  proto->mutable_time()->set_second(second);
}

MATCHER_P(EqualsProto, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;

class CollectUserDataActionTest : public content::RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    ON_CALL(mock_action_delegate_, GetPersonalDataManager)
        .WillByDefault(Return(&mock_personal_data_manager_));
    ON_CALL(mock_action_delegate_, GetWebsiteLoginFetcher)
        .WillByDefault(Return(&mock_website_login_fetcher_));
    ON_CALL(mock_action_delegate_, GetUserData)
        .WillByDefault(Return(&user_data_));
    ON_CALL(mock_action_delegate_, WriteUserData(_))
        .WillByDefault(Invoke(
            [this](base::OnceCallback<void(UserData*, UserData::FieldChange*)>
                       write_callback) {
              UserData::FieldChange field_change = UserData::FieldChange::NONE;
              std::move(write_callback).Run(&user_data_, &field_change);
            }));
    ON_CALL(mock_action_delegate_, CollectUserData(_))
        .WillByDefault(
            Invoke([this](CollectUserDataOptions* collect_user_data_options) {
              std::move(collect_user_data_options->confirm_callback)
                  .Run(&user_data_, &user_model_);
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
  UserData user_data_;
  UserModel user_model_;
};

TEST_F(CollectUserDataActionTest, FailsForMissingPrivacyText) {
  ActionProto action_proto;
  action_proto.mutable_collect_user_data();

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SucceedsForPrivacyTextPresent) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(false);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            user_data_.terms_and_conditions_ = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(
                  &CollectUserDataResultProto::is_terms_and_conditions_accepted,
                  true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, FailsForMissingTermsAcceptTextIfRequired) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_terms_require_review_text("terms review");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, FailsForMissingTermsReviewTextIfRequired) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SucceedsForCheckboxIfReviewTextMissing) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(true);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            user_data_.terms_and_conditions_ = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(
                  &CollectUserDataResultProto::is_terms_and_conditions_accepted,
                  true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SucceedsForAllTermsTextPresent) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_privacy_notice_text("privacy");
  collect_user_data_proto->set_request_terms_and_conditions(true);
  collect_user_data_proto->set_accept_terms_and_conditions_text(
      "terms and conditions");
  collect_user_data_proto->set_show_terms_as_checkbox(false);
  collect_user_data_proto->set_terms_require_review_text("terms review");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            user_data_.terms_and_conditions_ = ACCEPTED;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(
                  &CollectUserDataResultProto::is_terms_and_conditions_accepted,
                  true))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InfoSectionText) {
  const char kInfoSection[] = "Info section.";

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_info_section_text(kInfoSection);
  collect_user_data_proto->set_request_terms_and_conditions(false);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));
  EXPECT_CALL(
      mock_action_delegate_,
      CollectUserData(Pointee(Field(&CollectUserDataOptions::info_section_text,
                                    StrEq(kInfoSection)))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, PromptIsShown) {
  const char kPrompt[] = "Some message.";

  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_prompt(kPrompt);

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(kPrompt));
  EXPECT_CALL(callback_, Run(_));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, SelectLogin) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
  auto* login_option = login_details->add_login_options();
  login_option->mutable_password_manager();
  login_option->set_payload("payload");

  // Action should fetch the logins, but not the passwords.
  EXPECT_CALL(mock_website_login_fetcher_, OnGetLoginsForUrl(GURL(kFakeUrl), _))
      .Times(1);
  EXPECT_CALL(mock_website_login_fetcher_, OnGetPasswordForLogin(_, _))
      .Times(0);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            user_data_.login_choice_identifier_.assign(
                collect_user_data_options->login_choices[0].identifier);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
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
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
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

  EXPECT_CALL(mock_action_delegate_, CollectUserData(_)).Times(0);
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
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* login_details = collect_user_data_proto->mutable_login_details();
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

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            user_data_.selected_addresses_[kMemoryLocation] =
                std::make_unique<autofill::AutofillProfile>(contact_profile);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(callback_,
              Run(Pointee(AllOf(
                  Property(&ProcessedActionProto::status, ACTION_APPLIED),
                  Property(&ProcessedActionProto::collect_user_data_result,
                           Property(&CollectUserDataResultProto::payer_email,
                                    "marion@me.xyz"))))));

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(user_data_.has_selected_address(kMemoryLocation), true);
  auto* profile = user_data_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL),
            base::UTF8ToUTF16("Marion Mitchell Morrison"));
  EXPECT_EQ(profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER),
            base::UTF8ToUTF16("16505678910"));
  EXPECT_EQ(profile->GetRawInfo(autofill::EMAIL_ADDRESS),
            base::UTF8ToUTF16("marion@me.xyz"));
}

TEST_F(CollectUserDataActionTest, SelectPaymentMethod) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_request_payment_method(true);
  collect_user_data_proto->set_billing_address_name("billing_address");

  autofill::AutofillProfile billing_profile(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetProfileInfo(&billing_profile, "Marion", "Mitchell",
                                 "Morrison", "marion@me.xyz", "Fox",
                                 "123 Zoo St.", "unit 5", "Hollywood", "CA",
                                 "91601", "US", "16505678910");

  autofill::CreditCard credit_card(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(&credit_card, "Marion Mitchell",
                                    "4111 1111 1111 1111", "01", "2020",
                                    billing_profile.guid());

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.selected_card_ =
                std::make_unique<autofill::CreditCard>(credit_card);
            user_data_.selected_addresses_["billing_address"] =
                std::make_unique<autofill::AutofillProfile>(billing_profile);
            user_data_.succeed_ = true;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
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

  EXPECT_EQ(user_data_.selected_card_.get() != nullptr, true);
  EXPECT_THAT(user_data_.selected_card_->Compare(credit_card), Eq(0));
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

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            user_data_.selected_addresses_[kMemoryLocation] =
                std::make_unique<autofill::AutofillProfile>(contact_profile);
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
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

  EXPECT_EQ(user_data_.has_selected_address(kMemoryLocation), true);
  auto* profile = user_data_.selected_address(kMemoryLocation);
  EXPECT_EQ(profile->GetRawInfo(autofill::NAME_FULL),
            base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE"));
  EXPECT_EQ(
      profile->GetRawInfo(autofill::EMAIL_ADDRESS),
      base::UTF8ToUTF16("\xE8\x89\xBE\xE4\xB8\xBD\xE6\xA3\xAE@example.com"));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Contact) {
  UserData user_data;
  CollectUserDataOptions options;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  options.contact_details_name = "profile";
  user_data.selected_addresses_["profile"] =
      std::make_unique<autofill::AutofillProfile>(base::GenerateGUID(),
                                                  kFakeUrl);
  options.request_payer_email = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.selected_addresses_["profile"]->SetRawInfo(
      autofill::ServerFieldType::EMAIL_ADDRESS,
      base::UTF8ToUTF16("joedoe@example.com"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  options.request_payer_name = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.selected_addresses_["profile"]->SetRawInfo(
      autofill::ServerFieldType::NAME_FULL, base::UTF8ToUTF16("Joe Doe"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  options.request_payer_phone = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.selected_addresses_["profile"]->SetRawInfo(
      autofill::ServerFieldType::PHONE_HOME_WHOLE_NUMBER,
      base::UTF8ToUTF16("+1 23 456 789 01"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Payment) {
  UserData user_data;
  CollectUserDataOptions options;

  options.request_payment_method = true;
  options.billing_address_name = "billing_address";
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  // Valid credit card, but no billing address.
  user_data.selected_card_ =
      std::make_unique<autofill::CreditCard>(base::GenerateGUID(), kFakeUrl);
  autofill::test::SetCreditCardInfo(user_data.selected_card_.get(),
                                    "Marion Mitchell", "4111 1111 1111 1111",
                                    "01", "2020",
                                    /* billing_address_id = */ "");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  // Incomplete billing address.
  user_data.selected_addresses_["billing_address"] =
      std::make_unique<autofill::AutofillProfile>(base::GenerateGUID(),
                                                  kFakeUrl);
  autofill::test::SetProfileInfo(
      user_data.selected_addresses_["billing_address"].get(), "Marion",
      "Mitchell", "Morrison", "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA",
      /* zipcode = */ "", "US", "16505678910");
  user_data.selected_card_->set_billing_address_id(
      user_data.selected_addresses_["billing_address"]->guid());
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.selected_addresses_["billing_address"]->SetRawInfo(
      autofill::ADDRESS_HOME_ZIP, base::UTF8ToUTF16("91601"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  // Zip code is optional in Argentinian address.
  user_data.selected_addresses_["billing_address"]->SetRawInfo(
      autofill::ADDRESS_HOME_ZIP, base::UTF8ToUTF16(""));
  user_data.selected_addresses_["billing_address"]->SetRawInfo(
      autofill::ADDRESS_HOME_COUNTRY, base::UTF8ToUTF16("AR"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  options.require_billing_postal_code = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.selected_addresses_["billing_address"]->SetRawInfo(
      autofill::ADDRESS_HOME_ZIP, base::UTF8ToUTF16("B1675"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  // Expired credit card.
  user_data.selected_card_->SetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
                                       base::UTF8ToUTF16("2019"));
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Terms) {
  UserData user_data;
  CollectUserDataOptions options;

  options.accept_terms_and_conditions_text.assign("Accept T&C");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.terms_and_conditions_ = REQUIRES_REVIEW;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.terms_and_conditions_ = ACCEPTED;
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_Login) {
  UserData user_data;
  CollectUserDataOptions options;

  options.request_login_choice = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.login_choice_identifier_.assign("1");
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_ShippingAddress) {
  UserData user_data;
  CollectUserDataOptions options;
  options.request_shipping = true;
  options.shipping_address_name = "shipping_address";
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  // Incomplete address.
  user_data.selected_addresses_["shipping_address"] =
      std::make_unique<autofill::AutofillProfile>(base::GenerateGUID(),
                                                  kFakeUrl);
  autofill::test::SetProfileInfo(
      user_data.selected_addresses_["shipping_address"].get(), "Marion",
      "Mitchell", "Morrison", "marion@me.xyz", "Fox", "123 Zoo St.", "unit 5",
      "Hollywood", "CA",
      /* zipcode = */ "", "US", "16505678910");
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.selected_addresses_["shipping_address"]->SetRawInfo(
      autofill::ADDRESS_HOME_ZIP, base::UTF8ToUTF16("91601"));
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, UserDataComplete_DateTimeRange) {
  UserData user_data;
  CollectUserDataOptions options;
  options.request_date_time_range = true;
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  SetDateTimeProto(&user_data.date_time_range_start_, 2019, 12, 31, 10, 30, 0);
  SetDateTimeProto(&user_data.date_time_range_end_, 2019, 1, 28, 16, 0, 0);

  // Start date not before end date.
  EXPECT_FALSE(CollectUserDataAction::IsUserDataComplete(user_data, options));

  user_data.date_time_range_end_.mutable_date()->set_year(2020);
  EXPECT_TRUE(CollectUserDataAction::IsUserDataComplete(user_data, options));
}

TEST_F(CollectUserDataActionTest, SelectDateTimeRange) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* date_time_proto = collect_user_data_proto->mutable_date_time_range();
  SetDateTimeProto(date_time_proto->mutable_start(), 2019, 10, 21, 8, 0, 0);
  SetDateTimeProto(date_time_proto->mutable_end(), 2019, 11, 5, 16, 0, 0);
  SetDateTimeProto(date_time_proto->mutable_min(), 2019, 11, 5, 16, 0, 0);
  SetDateTimeProto(date_time_proto->mutable_max(), 2020, 11, 5, 16, 0, 0);
  date_time_proto->set_start_label("Pick up");
  date_time_proto->set_end_label("Return");
  date_time_proto->set_invalid_error_message("Return == Pick up");

  DateTimeProto actual_pickup_time;
  DateTimeProto actual_return_time;
  SetDateTimeProto(&actual_pickup_time, 2019, 10, 21, 7, 0, 0);
  SetDateTimeProto(&actual_return_time, 2019, 10, 25, 19, 0, 0);

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([&](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            user_data_.date_time_range_start_ = actual_pickup_time;
            user_data_.date_time_range_end_ = actual_return_time;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(
          AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                Property(&ProcessedActionProto::collect_user_data_result,
                         Property(&CollectUserDataResultProto::date_time_start,
                                  EqualsProto(actual_pickup_time))),
                Property(&ProcessedActionProto::collect_user_data_result,
                         Property(&CollectUserDataResultProto::date_time_end,
                                  EqualsProto(actual_return_time)))))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, StaticSectionValid) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* static_section =
      collect_user_data_proto->add_additional_prepended_sections();
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  static_section->set_title("Static section");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  static_section->mutable_static_text_section()->set_text("Lorem ipsum.");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }
}

TEST_F(CollectUserDataActionTest, TextInputSectionValid) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* text_input_section =
      collect_user_data_proto->add_additional_prepended_sections();
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  text_input_section->set_title("Text input section");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  auto* input_field =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field->set_value("12345");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  input_field->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  input_field->set_client_memory_key("code");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }

  // Duplicate input field fails due to duplicate memory key.
  *text_input_section->mutable_text_input_section()->add_input_fields() =
      *input_field;
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
    action.ProcessAction(callback_.Get());
  }

  text_input_section->mutable_text_input_section()
      ->mutable_input_fields(1)
      ->set_client_memory_key("something else");
  {
    CollectUserDataAction action(&mock_action_delegate_, action_proto);
    EXPECT_CALL(
        callback_,
        Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
    action.ProcessAction(callback_.Get());
  }
}

TEST_F(CollectUserDataActionTest, TextInputSectionWritesToClientMemory) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            user_data_.additional_values_["key2"] = "modified";
            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  auto* text_input_section =
      collect_user_data_proto->add_additional_prepended_sections();
  text_input_section->set_title("Text input section");

  auto* input_field_1 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_1->set_value("initial");
  input_field_1->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_1->set_client_memory_key("key1");

  auto* input_field_2 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_2->set_value("initial");
  input_field_2->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_2->set_client_memory_key("key2");

  auto* input_field_3 =
      text_input_section->mutable_text_input_section()->add_input_fields();
  input_field_3->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_3->set_client_memory_key("key3");

  EXPECT_FALSE(user_data_.has_additional_value("key1"));
  EXPECT_FALSE(user_data_.has_additional_value("key2"));
  EXPECT_FALSE(user_data_.has_additional_value("key3"));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(
              &ProcessedActionProto::collect_user_data_result,
              Property(&CollectUserDataResultProto::set_text_input_memory_keys,
                       UnorderedElementsAre("key1", "key2")))))));
  action.ProcessAction(callback_.Get());
  EXPECT_EQ(*user_data_.additional_value("key1"), "initial");
  EXPECT_EQ(*user_data_.additional_value("key2"), "modified");
  EXPECT_EQ(*user_data_.additional_value("key3"), "");
}

TEST_F(CollectUserDataActionTest, AllowedBasicCardNetworks) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  collect_user_data_proto->set_billing_address_name("billing_address");

  std::string kSupportedBasicCardNetworks[] = {"amex", "diners",   "discover",
                                               "elo",  "jcb",      "mastercard",
                                               "mir",  "unionpay", "visa"};

  for (const auto& network : kSupportedBasicCardNetworks) {
    *collect_user_data_proto->add_supported_basic_card_networks() = network;
  }

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([this](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;

            user_data_.selected_addresses_["billing_address"] =
                std::make_unique<autofill::AutofillProfile>(
                    base::GenerateGUID(), kFakeUrl);
            autofill::test::SetProfileInfo(
                user_data_.selected_addresses_["billing_address"].get(),
                "Marion", "Mitchell", "Morrison", "marion@me.xyz", "Fox",
                "123 Zoo St.", "unit 5", "Hollywood", "CA", "96043", "US",
                "16505678910");

            user_data_.selected_card_ = std::make_unique<autofill::CreditCard>(
                base::GenerateGUID(), kFakeUrl);
            autofill::test::SetCreditCardInfo(
                user_data_.selected_card_.get(), "Marion Mitchell",
                "4111 1111 1111 1111", "01", "2020",
                user_data_.selected_addresses_["billing_address"]->guid());

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InvalidBasicCardNetworks) {
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);

  *collect_user_data_proto->add_supported_basic_card_networks() = "visa";
  *collect_user_data_proto->add_supported_basic_card_networks() =
      "unknown_network";

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, OverwriteExistingUserData) {
  // Set previous user data state.
  user_data_.terms_and_conditions_ = ACCEPTED;
  user_data_.additional_values_["key1"] = "val1";
  user_data_.additional_values_["key2"] = "val2";
  user_data_.additional_values_["key3"] = "val3";

  // Set options.
  ActionProto action_proto;
  auto* collect_user_data_proto = action_proto.mutable_collect_user_data();
  collect_user_data_proto->set_request_terms_and_conditions(false);
  auto* prepended_section =
      collect_user_data_proto->add_additional_prepended_sections();
  prepended_section->set_title("Text input section");

  auto* input_field_1 =
      prepended_section->mutable_text_input_section()->add_input_fields();
  input_field_1->set_value("initial");
  input_field_1->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_1->set_client_memory_key("key1");

  auto* appended_section =
      collect_user_data_proto->add_additional_appended_sections();
  appended_section->set_title("Text input section 2");
  auto* input_field_2 =
      appended_section->mutable_text_input_section()->add_input_fields();
  input_field_2->set_value("initial");
  input_field_2->set_input_type(TextInputProto::INPUT_ALPHANUMERIC);
  input_field_2->set_client_memory_key("key2");

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([](CollectUserDataOptions* collect_user_data_options) {
            // do not call confirm_callback since we are only looking to test
            // OnShowToUser.
            // Calling confirm_callback then calls OnGetUserData which changes
            // the user_data_.
          }));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());

  EXPECT_EQ(user_data_.terms_and_conditions_, NOT_SELECTED);
  EXPECT_EQ(user_data_.additional_values_["key1"], "initial");
  EXPECT_EQ(user_data_.additional_values_["key2"], "initial");
  EXPECT_EQ(user_data_.additional_values_["key3"], "val3");
}

TEST_F(CollectUserDataActionTest, AttachesProfiles) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;
            user_data_.selected_addresses_[kMemoryLocation] =
                std::make_unique<autofill::AutofillProfile>(profile);

            EXPECT_THAT(user_data_.available_profiles_, SizeIs(1));
            EXPECT_EQ(user_data_.available_profiles_[0]->Compare(profile), 0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("contact");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InitialSelectsProfileAndShippingAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;

            EXPECT_EQ(
                user_data_.selected_addresses_["profile"]->Compare(profile), 0);
            EXPECT_EQ(
                user_data_.selected_addresses_["shipping-address"]->Compare(
                    profile),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_shipping_address_name("shipping-address");
  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("profile");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InitialSelectsProfileFromDefaultEmail) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile_a;
  autofill::test::SetProfileInfo(&profile_a, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  autofill::AutofillProfile profile_b;
  autofill::test::SetProfileInfo(&profile_b, "Berta", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(Return(
          std::vector<autofill::AutofillProfile*>({&profile_a, &profile_b})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;

            EXPECT_EQ(
                user_data_.selected_addresses_["profile"]->Compare(profile_b),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  auto* contact_details = user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_request_payer_email(true);
  contact_details->set_contact_details_name("profile");

  ON_CALL(mock_action_delegate_, GetAccountEmailAddress())
      .WillByDefault(Return("berta.west@gmail.com"));

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, KeepsSelectedProfileAndShippingAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;

            EXPECT_EQ(
                user_data_.selected_addresses_["profile"]->Compare(profile), 0);
            EXPECT_EQ(
                user_data_.selected_addresses_["shipping_address"]->Compare(
                    profile),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->set_shipping_address_name("shipping_address");
  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_request_payer_name(true);
  contact_details->set_contact_details_name("profile");

  // Set previous user data.
  user_data_.selected_addresses_["profile"] =
      std::make_unique<autofill::AutofillProfile>(profile);
  user_data_.selected_addresses_["shipping_address"] =
      std::make_unique<autofill::AutofillProfile>(profile);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ResetsContactAndShippingIfNoLongerInList) {
  ON_CALL(mock_personal_data_manager_, IsAutofillProfileEnabled)
      .WillByDefault(Return(true));

  autofill::AutofillProfile profile;
  autofill::test::SetProfileInfo(&profile, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  ON_CALL(mock_personal_data_manager_, GetProfiles)
      .WillByDefault(
          Return(std::vector<autofill::AutofillProfile*>({&profile})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(user_data_.selected_addresses_["profile"], nullptr);
            EXPECT_EQ(
                user_data_.selected_addresses_["shipping_address"]->Compare(
                    profile),
                0);

            // Do not call the callback. We're only interested in the state.
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->mutable_contact_details();
  collect_user_data->set_shipping_address_name("shipping_address");
  auto* contact_details = collect_user_data->mutable_contact_details();
  contact_details->set_contact_details_name("profile");

  // Set previous user data.
  autofill::AutofillProfile selected_profile;
  autofill::test::SetProfileInfo(&selected_profile, "Berta", "", "West",
                                 "berta.west@gmail.com", "", "", "", "", "", "",
                                 "", "");

  user_data_.selected_addresses_["profile"] =
      std::make_unique<autofill::AutofillProfile>(selected_profile);
  user_data_.selected_addresses_["shipping_address"] =
      std::make_unique<autofill::AutofillProfile>(selected_profile);

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, AttachesCreditCardsWithAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;

            EXPECT_THAT(user_data_.available_payment_instruments_, SizeIs(1));
            EXPECT_EQ(
                user_data_.available_payment_instruments_[0]->card->Compare(
                    card_with_address),
                0);
            EXPECT_EQ(user_data_.available_payment_instruments_[0]
                          ->billing_address->Compare(billing_address),
                      0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  user_data->add_supported_basic_card_networks("visa");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, AttachesCreditCardsWithoutAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::CreditCard card_without_address;
  autofill::test::SetCreditCardInfo(&card_without_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_without_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;

            EXPECT_THAT(user_data_.available_payment_instruments_, SizeIs(1));
            EXPECT_EQ(
                user_data_.available_payment_instruments_[0]->card->Compare(
                    card_without_address),
                0);
            EXPECT_EQ(user_data_.available_payment_instruments_[0]
                          ->billing_address.get(),
                      nullptr);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);
  user_data->add_supported_basic_card_networks("visa");

  EXPECT_CALL(mock_personal_data_manager_, GetProfileByGUID(_)).Times(0);
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, AttachesCreditCardsForEmptyNetworksList) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::CreditCard card;
  autofill::test::SetCreditCardInfo(&card, "Adam West", "4111111111111111", "1",
                                    "2050",
                                    /* billing_address_id= */ "");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(Return(std::vector<autofill::CreditCard*>({&card})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;

            EXPECT_THAT(user_data_.available_payment_instruments_, SizeIs(1));
            EXPECT_EQ(
                user_data_.available_payment_instruments_[0]->card->Compare(
                    card),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, &user_model_);
          }));

  ActionProto action_proto;
  auto* user_data = action_proto.mutable_collect_user_data();
  user_data->set_request_terms_and_conditions(false);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, InitialSelectsCardAndAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;

            EXPECT_EQ(user_data_.selected_card_->Compare(card_with_address), 0);
            EXPECT_EQ(
                user_data_.selected_addresses_["billing_address"]->Compare(
                    billing_address),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->add_supported_basic_card_networks("visa");
  collect_user_data->set_request_payment_method(true);
  collect_user_data->set_billing_address_name("billing_address");

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, KeepsSelectedCardAndAddress) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            user_data_.succeed_ = true;

            EXPECT_EQ(user_data_.selected_card_->Compare(card_with_address), 0);
            EXPECT_EQ(
                user_data_.selected_addresses_["billing_address"]->Compare(
                    billing_address),
                0);

            std::move(collect_user_data_options->confirm_callback)
                .Run(&user_data_, nullptr);
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->add_supported_basic_card_networks("visa");
  collect_user_data->set_billing_address_name("billing_address");

  // Set previous user data.
  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(card_with_address);
  user_data_.selected_addresses_["billing_address"] =
      std::make_unique<autofill::AutofillProfile>(billing_address);

  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, ACTION_APPLIED))));
  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

TEST_F(CollectUserDataActionTest, ResetsCardAndAddressIfNoLongerInList) {
  ON_CALL(mock_personal_data_manager_, IsAutofillCreditCardEnabled)
      .WillByDefault(Return(true));
  ON_CALL(mock_personal_data_manager_, ShouldSuggestServerCards)
      .WillByDefault(Return(true));

  autofill::AutofillProfile billing_address;
  autofill::test::SetProfileInfo(&billing_address, "Adam", "", "West",
                                 "adam.west@gmail.com", "", "Baker Street 221b",
                                 "", "London", "", "WC2N 5DU", "UK", "+44");

  ON_CALL(mock_personal_data_manager_, GetProfileByGUID("GUID"))
      .WillByDefault(Return(&billing_address));

  autofill::CreditCard card_with_address;
  autofill::test::SetCreditCardInfo(&card_with_address, "Adam West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "GUID");

  ON_CALL(mock_personal_data_manager_, GetCreditCards())
      .WillByDefault(
          Return(std::vector<autofill::CreditCard*>({&card_with_address})));

  ON_CALL(mock_action_delegate_, CollectUserData(_))
      .WillByDefault(
          Invoke([=](CollectUserDataOptions* collect_user_data_options) {
            EXPECT_EQ(user_data_.selected_card_, nullptr);
            EXPECT_EQ(user_data_.selected_addresses_["billing_address"],
                      nullptr);

            // Do not call the callback. We're only interested in the state.
          }));

  ActionProto action_proto;
  auto* collect_user_data = action_proto.mutable_collect_user_data();
  collect_user_data->set_request_terms_and_conditions(false);
  collect_user_data->add_supported_basic_card_networks("visa");
  collect_user_data->set_billing_address_name("billing_address");

  // Set previous user data.
  autofill::CreditCard selected_card;
  autofill::test::SetCreditCardInfo(&selected_card, "Berta West",
                                    "4111111111111111", "1", "2050",
                                    /* billing_address_id= */ "");
  autofill::AutofillProfile selected_address;
  autofill::test::SetProfileInfo(
      &selected_address, "Berta", "", "West", "berta.west@gmail.com", "",
      "Baker Street 221b", "", "London", "", "WC2N 5DU", "UK", "+44");

  user_data_.selected_card_ =
      std::make_unique<autofill::CreditCard>(selected_card);
  user_data_.selected_addresses_["billing_address"] =
      std::make_unique<autofill::AutofillProfile>(selected_address);

  CollectUserDataAction action(&mock_action_delegate_, action_proto);
  action.ProcessAction(callback_.Get());
}

}  // namespace
}  // namespace autofill_assistant
